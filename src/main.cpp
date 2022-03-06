#include <Magnum/Magnum.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/MeshTools/GenerateNormals.h>
#include <Magnum/Primitives/UVSphere.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/Shaders/Generic.h>
#include <Magnum/Shaders/visibility.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/ImageView.h>
#include <Corrade/Containers/Reference.h>
#include <Magnum/GL/Version.h>
#include <Magnum/GL/DebugOutput.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Platform/GLContext.h>
#include <Magnum/Shaders/VertexColor.h>
#include <GLFW/glfw3.h>
#include <Magnum/GlmIntegration/GtcIntegration.h>
#include <Magnum/GlmIntegration/GtxIntegration.h>
#include <Magnum/GlmIntegration/Integration.h>
#include <MagnumPlugins/StbImageImporter/StbImageImporter.h>
#include <MagnumPlugins/StbImageImporter/configure.h>
#include <spdlog/spdlog.h>

using namespace Magnum;

class PBRShader: public GL::AbstractShaderProgram 
{
public:
    typedef Shaders::Generic3D::Position Position; //0
    typedef Shaders::Generic3D::TextureCoordinates TextureCoord; //1
    typedef Shaders::Generic3D::Tangent4 Tangent4; //3
    typedef Shaders::Generic3D::Bitangent Bitangent; //4
    typedef Shaders::Generic3D::Normal Normal; //5

    explicit PBRShader()
    {
        MAGNUM_ASSERT_GL_VERSION_SUPPORTED(GL::Version::GL450);

        GL::Shader vert(GL::Version::GL450, GL::Shader::Type::Vertex);
        GL::Shader frag(GL::Version::GL450, GL::Shader::Type::Fragment);

        vert.addSource(R"(
            layout(location = 0) in vec3 position;
            layout(location = 1) in vec2 tex_coord;
            layout(location = 3) in vec4 tangent4;
            layout(location = 5) in vec3 normal;

            out vec2 frag_tex_coord;
            out mat3 frag_TBN;
            out vec3 frag_pos;

            uniform mat4 model_matrix;
            uniform mat4 view_matrix;
            uniform mat4 proj_matrix;
            uniform mat3 normal_matrix;

            uniform sampler2D height_texture;
            uniform float height_factor = 0.5;

            void main()
            {   
                vec3 N = normalize(normal_matrix * normal);
                vec3 T = normalize(normal_matrix * tangent4.xyz);
                vec3 B = normalize(tangent4.a * cross(N, T));
                frag_TBN = mat3(T, B, N);

                mat4 mv_matrix = view_matrix * model_matrix;

                vec4 pos = mv_matrix * vec4(position, 1.0);
                pos = proj_matrix * vec4(pos.xyz + N * texture2D(height_texture, tex_coord).r * height_factor, 1.0);
                gl_Position = pos;

                frag_tex_coord = tex_coord;
            }
        )");

        frag.addSource(R"(
            #define BASIC_MODE 0
            #define ALBEDO_MODE 1
            #define ROUGHNESS_MODE 2
            #define METALLIC_MODE 3
            #define NORMAL_MODE 4
            #define AO_MODE 5
            uniform int render_mode = BASIC_MODE;

            in vec2 frag_tex_coord;
            in mat3 frag_TBN;
            in vec3 frag_pos;
            out vec4 fragment_color;

            uniform sampler2D albedo_texture;
            uniform sampler2D roughness_texture;
            uniform sampler2D metallic_texture;
            uniform sampler2D normal_texture;
            uniform sampler2D ao_texture;

            uniform float albedo_factor = 1.0;
            uniform float roughness_factor = 1.0;
            uniform float metallic_factor = 1.0;
            uniform float normal_factor = 1.0;
            uniform float ao_factor = 1.0;

            uniform vec3 light_direction = vec3(0.0, -0.5, -0.5);
            uniform vec3 light_color = vec3(1.0);

            void main()
            {   
                const float gamma = 2.2;
                vec4 albedo = texture2D(albedo_texture, frag_tex_coord) * albedo_factor;
                float roughness = texture2D(roughness_texture, frag_tex_coord).r * roughness_factor;
                float metallic = texture2D(metallic_texture, frag_tex_coord).r * metallic_factor;

                vec3 normal = normalize(texture2D(normal_texture, frag_tex_coord).rgb * 2.0 - vec3(1.0));
                normal = normalize(frag_TBN * normal) * (gl_FrontFacing ? 1.0 : -1.0) * normal_factor;

                vec3 ao = texture2D(ao_texture, frag_tex_coord).rgb * ao_factor;
            
                switch(render_mode)
                {
                    case ROUGHNESS_MODE:
                        fragment_color = vec4(vec3(roughness), 1.0);
                        break;
                    case METALLIC_MODE:
                        fragment_color = vec4(vec3(metallic), 1.0);
                        break;
                    case NORMAL_MODE:
                        fragment_color = vec4(normal * 0.5 + 0.5, 1.0);
                        break;
                    case AO_MODE:
                        fragment_color = vec4(ao, 1.0);
                        break;
                    case ALBEDO_MODE:
                        fragment_color = albedo;
                        break;
                    case BASIC_MODE:
                    default:
                        float NdotL = max(dot(normal, normalize(-light_direction)), 0.1);
                        fragment_color = (0.1 + NdotL * vec4(light_color, 1.0) * vec4(ao, 1.0) * 0.9) * albedo;

                        break;
                }

                fragment_color.rgb = pow(fragment_color.rgb, vec3(1.0 / gamma));
                //fragment_color = vec4(normal, 1.0);
            }
        )");

        CORRADE_INTERNAL_ASSERT_OUTPUT(GL::Shader::compile({ vert, frag }));
        attachShaders({ vert, frag }); 
        CORRADE_INTERNAL_ASSERT_OUTPUT(link());

        auto [status, message] = validate();

        if (status)
        {
            spdlog::info("PBRShader compiled successfully");
        }
        else
        {
            spdlog::error("Unable to compile PBRShader");
        }


        if (!message.empty())
        {
            if (status)
            {
                spdlog::info(message);
            }
            else
            {
                spdlog::error(message);
            }
        }

        //...
        bindAttributeLocation(Position::Location, "position");
        bindAttributeLocation(TextureCoord::Location, "tex_coord");
        bindAttributeLocation(Normal::Location, "normal");
        bindAttributeLocation(Tangent4::Location, "tangent4");
        bindAttributeLocation(Bitangent::Location, "bitangent");

        model_matrix_uniform = uniformLocation("model_matrix");
        view_matrix_uniform = uniformLocation("view_matrix");
        proj_matrix_uniform = uniformLocation("proj_matrix");
        normal_matrix_uniform = uniformLocation("normal_matrix");

        light_direction_uniform = uniformLocation("light_direction");
        light_color_uniform = uniformLocation("light_color");

        albedo_factor_uniform = uniformLocation("albedo_factor");
        roughness_factor_uniform = uniformLocation("roughness_factor");
        metallic_factor_uniform = uniformLocation("metallic_factor");
        normal_factor_uniform = uniformLocation("normal_factor");
        ao_factor_uniform = uniformLocation("ao_factor");
        height_factor_uniform = uniformLocation("height_factor");

        render_mode_uniform = uniformLocation("render_mode");

        setUniform(uniformLocation("albedo_texture"), AlbedoUnit);
        setUniform(uniformLocation("roughness_texture"), RoughnessUnit);
        setUniform(uniformLocation("metallic_texture"), MetallicUnit);
        setUniform(uniformLocation("normal_texture"), NormalUnit);
        setUniform(uniformLocation("ao_texture"), AOUnit);
        setUniform(uniformLocation("height_texture"), HeightUnit);
    }

    PBRShader& set_model_matrix(const Matrix4& mtx)
    {
        setUniform(model_matrix_uniform, mtx);
        return *this;
    }

    PBRShader& set_view_matrix(const Matrix4& mtx)
    {
        setUniform(view_matrix_uniform, mtx);
        return *this;
    }

    PBRShader& set_proj_matrix(const Matrix4& mtx)
    {
        setUniform(proj_matrix_uniform, mtx);
        return *this;
    }

    PBRShader& set_normal_matrix(const Matrix3x3& mtx)
    {
        setUniform(normal_matrix_uniform, mtx);
        return *this;
    }

    PBRShader& set_light_direction(const Vector3& dir)
    {
        setUniform(light_direction_uniform, dir);
        return *this;
    }

    PBRShader& set_light_color(const Vector3& color)
    {
        setUniform(light_color_uniform, color);
        return *this;
    }

    PBRShader& set_albedo_factor(float factor)
    {
        setUniform(albedo_factor_uniform, factor);
        return *this;
    }

    PBRShader& set_roughness_factor(float factor)
    {
        setUniform(roughness_factor_uniform, factor);
        return *this;
    }

    PBRShader& set_metallic_factor(float factor)
    {
        setUniform(metallic_factor_uniform, factor);
        return *this;
    }

    PBRShader& set_normal_factor(float factor)
    {
        setUniform(normal_factor_uniform, factor);
        return *this;
    }

    PBRShader& set_ao_factor(float factor)
    {
        setUniform(ao_factor_uniform, factor);
        return *this;
    }

    PBRShader& set_height_factor(float factor)
    {
        setUniform(height_factor_uniform, factor);
        return *this;
    }

    PBRShader& bind_albedo_texture(GL::Texture2D& tex)
    {
        tex.bind(AlbedoUnit);
        return *this;
    }

    PBRShader& bind_roughness_texture(GL::Texture2D& tex)
    {
        tex.bind(RoughnessUnit);
        return *this;
    }

    PBRShader& bind_metallic_texture(GL::Texture2D& tex)
    {
        tex.bind(MetallicUnit);
        return *this;
    }

    PBRShader& bind_normal_texture(GL::Texture2D& tex)
    {
        tex.bind(NormalUnit);
        return *this;
    }

    PBRShader& bind_ao_texture(GL::Texture2D& tex)
    {
        tex.bind(AOUnit);
        return *this;
    }

    PBRShader& bind_height_texture(GL::Texture2D& tex)
    {
        tex.bind(HeightUnit);
        return *this;
    }

    PBRShader& set_render_mode(int mode)
    {
        setUniform(render_mode_uniform, mode % RENDER_MODE_COUNT);
        return *this;
    }

    static const int RENDER_MODE_COUNT = 6;
private:
    enum : Int //0..n
    {
        AlbedoUnit,
        RoughnessUnit,
        MetallicUnit,
        NormalUnit,
        AOUnit,
        HeightUnit
    };

    Int model_matrix_uniform,
        view_matrix_uniform,
        proj_matrix_uniform,
        normal_matrix_uniform;

    Int light_direction_uniform,
        light_color_uniform,
        render_mode_uniform;

    Int albedo_factor_uniform,
        roughness_factor_uniform,
        metallic_factor_uniform,
        normal_factor_uniform,
        ao_factor_uniform,
        height_factor_uniform;
};

int main(int argc, char** argv)
{
    if (!glfwInit())
    {
        spdlog::error("Can't initialize GLFW");
        return -1;
    }

    constexpr glm::ivec2 window_size{ 1024, 1024 };
    constexpr float aspect = window_size.x / float(window_size.y);
    constexpr float fov = glm::radians(45.0f);
    const glm::mat4 proj = glm::perspective(fov, aspect, 0.01f, 100.0f);

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 8);
    GLFWwindow* const window = glfwCreateWindow(window_size.x, window_size.y, "Demo", nullptr, nullptr);

    if (!window)
    {
        spdlog::error("Can't create window");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    {
        Platform::GLContext ctx{ argc, argv };

        GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
        GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);
        GL::Renderer::enable(GL::Renderer::Feature::Multisampling);

        GL::Renderer::enable(GL::Renderer::Feature::DebugOutput);
        GL::Renderer::enable(GL::Renderer::Feature::DebugOutputSynchronous);
        GL::DebugOutput::setDefaultCallback();
        /* Disable rather spammy "Buffer detailed info" debug messages on NVidia drivers */
        GL::DebugOutput::setEnabled(GL::DebugOutput::Source::Api, GL::DebugOutput::Type::Other, { 131185 }, false);

        /* Setup the colored triangle */
        using namespace Math::Literals;

        struct TriangleVertex 
        {
            Vector3 position;
            Color3 color;
        };

        const TriangleVertex vertices[]
        {
            {{-0.5f, -0.5f, 0.0f}, 0xff0000_rgbf},
            {{ 0.5f, -0.5f, 0.0f}, 0x00ff00_rgbf},
            {{ 0.0f,  0.5f, 0.0f}, 0x0000ff_rgbf}
        };

        GL::Mesh mesh;
        mesh.setCount(Containers::arraySize(vertices))
            .addVertexBuffer(GL::Buffer{ vertices }, 0,
                Shaders::VertexColor3D::Position{},
                Shaders::VertexColor3D::Color3{});

        GL::Mesh sphere_mesh = MeshTools::compile(Primitives::uvSphereSolid(128, 128, Primitives::UVSphereFlag::Tangents | Primitives::UVSphereFlag::TextureCoordinates), 
            MeshTools::CompileFlag::GenerateSmoothNormals);

        PBRShader pbr_shader; //TODO: pbr_shader -> shader
        //pbr_shader.draw(sphere_mesh);

        CORRADE_PLUGIN_IMPORT(StbImageImporter);
        PluginManager::Manager<Trade::AbstractImporter> manager;
        auto image_loader = manager.instantiate("StbImageImporter");

        image_loader->openFile("data/albedo.png");
        auto albedo_image = image_loader->image2D(0);
        CORRADE_INTERNAL_ASSERT(albedo_image);
        GL::Texture2D albedo_texture;
        albedo_texture.setWrapping(GL::SamplerWrapping::ClampToEdge)
            .setMagnificationFilter(GL::SamplerFilter::Linear)
            .setMinificationFilter(GL::SamplerFilter::Linear, GL::SamplerMipmap::Linear)
            .setStorage(1, GL::textureFormat(albedo_image->format()), albedo_image->size())
            .setMaxAnisotropy(GL::Sampler::maxMaxAnisotropy())
            .setSrgbDecode(true)
            .setSubImage(0, {}, *albedo_image)
            .generateMipmap();

        image_loader->openFile("data/ao.png");
        auto ao_image = image_loader->image2D(0);
        CORRADE_INTERNAL_ASSERT(ao_image);
        GL::Texture2D ao_texture;
        ao_texture.setWrapping(GL::SamplerWrapping::ClampToEdge)
            .setMagnificationFilter(GL::SamplerFilter::Linear)
            .setMinificationFilter(GL::SamplerFilter::Linear, GL::SamplerMipmap::Linear)
            .setStorage(1, GL::textureFormat(ao_image->format()), ao_image->size())
            .setMaxAnisotropy(GL::Sampler::maxMaxAnisotropy())
            .setSrgbDecode(false)
            .setSubImage(0, {}, * ao_image)
            .generateMipmap();

        image_loader->openFile("data/metallic.png");
        auto metallic_image = image_loader->image2D(0);
        CORRADE_INTERNAL_ASSERT(metallic_image);
        GL::Texture2D metallic_texture;
        metallic_texture.setWrapping(GL::SamplerWrapping::ClampToEdge)
            .setMagnificationFilter(GL::SamplerFilter::Linear)
            .setMinificationFilter(GL::SamplerFilter::Linear, GL::SamplerMipmap::Linear)
            .setStorage(1, GL::textureFormat(metallic_image->format()), metallic_image->size())
            .setMaxAnisotropy(GL::Sampler::maxMaxAnisotropy())
            .setSrgbDecode(false)
            .setSubImage(0, {}, * metallic_image)
            .generateMipmap();

        image_loader->openFile("data/normal.png");
        auto normal_image = image_loader->image2D(0);
        CORRADE_INTERNAL_ASSERT(normal_image);
        GL::Texture2D normal_texture;
        normal_texture.setWrapping(GL::SamplerWrapping::ClampToEdge)
            .setMagnificationFilter(GL::SamplerFilter::Linear)
            .setMinificationFilter(GL::SamplerFilter::Linear, GL::SamplerMipmap::Linear)
            .setStorage(1, GL::textureFormat(normal_image->format()), normal_image->size())
            .setMaxAnisotropy(GL::Sampler::maxMaxAnisotropy())
            .setSrgbDecode(false)
            .setSubImage(0, {}, * normal_image)
            .generateMipmap();

        image_loader->openFile("data/roughness.png");
        auto roughness_image = image_loader->image2D(0);
        CORRADE_INTERNAL_ASSERT(roughness_image);
        GL::Texture2D roughness_texture;
        roughness_texture.setWrapping(GL::SamplerWrapping::ClampToEdge)
            .setMagnificationFilter(GL::SamplerFilter::Linear)
            .setMinificationFilter(GL::SamplerFilter::Linear, GL::SamplerMipmap::Linear)
            .setStorage(1, GL::textureFormat(roughness_image->format()), roughness_image->size())
            .setMaxAnisotropy(GL::Sampler::maxMaxAnisotropy())
            .setSrgbDecode(false)
            .setSubImage(0, {}, * roughness_image)
            .generateMipmap();

        image_loader->openFile("data/height.png");
        auto height_image = image_loader->image2D(0);
        CORRADE_INTERNAL_ASSERT(height_image);
        GL::Texture2D height_texture;
        height_texture.setWrapping(GL::SamplerWrapping::ClampToEdge)
            .setMagnificationFilter(GL::SamplerFilter::Linear)
            .setMinificationFilter(GL::SamplerFilter::Linear, GL::SamplerMipmap::Linear)
            .setStorage(1, GL::textureFormat(height_image->format()), height_image->size())
            .setMaxAnisotropy(GL::Sampler::maxMaxAnisotropy())
            .setSrgbDecode(false)
            .setSubImage(0, {}, * height_image)
            .generateMipmap();

        spdlog::info("Initialization successful");

        while (!glfwWindowShouldClose(window)) 
        {
            static int last_key_state = GLFW_RELEASE;
            static int current_mode = 0;

            if (last_key_state == GLFW_RELEASE)
            {
                if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
                {
                    last_key_state = GLFW_PRESS;
                    current_mode = (current_mode + 1) % PBRShader::RENDER_MODE_COUNT;
                    spdlog::info("Current mode {}", current_mode);
                }
            }

            glm::vec3 light_dir;
            {
                glm::dvec2 dpos;
                glfwGetCursorPos(window, &dpos.x, &dpos.y);
                if (dpos.x < 0.0 || dpos.x > window_size.x
                    || dpos.y < 0.0 || dpos.y > window_size.y)
                {
                    dpos = glm::dvec2(window_size) / glm::dvec2(2.0);
                }

                dpos /= window_size;
                dpos.x = 1.0 - dpos.x;
                dpos = dpos * 2.0 - 1.0;

                light_dir = glm::normalize(glm::vec3(dpos * 2.0, -1.0));
            }
            
            last_key_state = glfwGetKey(window, GLFW_KEY_SPACE);

            const glm::mat4 model = glm::rotate(glm::mat4(1.0), glm::radians(float(40.0f * glfwGetTime())), glm::vec3(0, 1, 0)) * glm::scale(glm::mat4(1.0), glm::vec3(5.0f));
            const glm::mat4 view = glm::lookAt(glm::vec3(0.0, 0.0, 20.0), glm::vec3(0.0), glm::vec3(0.0, 1.0, 0.0));

            GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);
            pbr_shader.set_model_matrix(Matrix4(model))
                .set_view_matrix(Matrix4(view))
                .set_proj_matrix(Matrix4(proj))
                .set_normal_matrix(Matrix4(view * model).normalMatrix())
                .bind_albedo_texture(albedo_texture)
                .bind_ao_texture(ao_texture)
                .bind_metallic_texture(metallic_texture)
                .bind_normal_texture(normal_texture)
                .bind_roughness_texture(roughness_texture)
                .bind_height_texture(height_texture)
                .set_light_direction(Vector3(light_dir))
                .set_render_mode(current_mode)
                .draw(sphere_mesh);
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        glfwTerminate();
    }
    return 0;
}