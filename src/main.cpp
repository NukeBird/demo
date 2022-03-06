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
#include <spdlog/spdlog.h>

using namespace Magnum;

class PBRShader: public GL::AbstractShaderProgram 
{
public:
    typedef Shaders::Generic3D::Position Position; //0
    typedef Shaders::Generic3D::TextureCoordinates TextureCoord; //1
    typedef Shaders::Generic3D::Tangent Tangent; //3
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
            layout(location = 3) in vec3 tangent;
            layout(location = 4) in vec3 bitangent;
            layout(location = 5) in vec3 normal;

            uniform mat4 model_matrix;
            uniform mat4 view_matrix;
            uniform mat4 proj_matrix;
            uniform mat4 normal_matrix;

            void main()
            {   
                mat4 mvp_matrix = proj_matrix * view_matrix * model_matrix;
                gl_Position = mvp_matrix * vec4(position, 1.0);
            }
        )");

        frag.addSource(R"(
            out vec4 fragment_color;

            layout(location = 0) uniform sampler2D albedo_texture;
            layout(location = 1) uniform sampler2D roughness_texture;
            layout(location = 2) uniform sampler2D metallic_texture;
            layout(location = 3) uniform sampler2D normal_texture;
            layout(location = 4) uniform sampler2D ao_texture;

            uniform vec3 light_direction_uniform = vec3(0.0, -0.5, -0.5);
            uniform vec3 light_color_uniform = vec3(1.0, 1.0, 1.0);
            uniform float albedo_factor_uniform = 1.0;
            uniform float roughness_factor_uniform = 1.0;
            uniform float metallic_factor_uniform = 1.0;
            uniform float normal_factor_uniform = 1.0;
            uniform float ao_factor_uniform = 1.0;

            void main()
            {   
                fragment_color = vec4(0.0, 1.0, 0.0, 1.0);
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
        bindAttributeLocation(Tangent::Location, "tangent");
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
    }

    PBRShader& set_model_matrix(const glm::mat4& mtx)
    {
        setUniform(model_matrix_uniform, (Matrix4)mtx);
        return *this;
    }

    PBRShader& set_view_matrix(const glm::mat4& mtx)
    {
        setUniform(view_matrix_uniform, (Matrix4)mtx);
        return *this;
    }

    PBRShader& set_proj_matrix(const glm::mat4& mtx)
    {
        setUniform(proj_matrix_uniform, (Matrix4)mtx);
        return *this;
    }

    PBRShader& set_normal_matrix(const glm::mat4& mtx)
    {
        setUniform(normal_matrix_uniform, (Matrix4)mtx);
        return *this;
    }

    PBRShader& set_light_direction(const glm::vec3& dir)
    {
        setUniform(light_direction_uniform, (Vector3)dir);
        return *this;
    }

    PBRShader& set_light_color(const glm::vec3& color)
    {
        setUniform(light_color_uniform, (Vector3)color);
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
private:
    enum : Int //0..n
    {
        AlbedoUnit,
        RoughnessUnit,
        MetallicUnit,
        NormalUnit,
        AOUnit
    };


    Int model_matrix_uniform,
        view_matrix_uniform,
        proj_matrix_uniform,
        normal_matrix_uniform;

    Int light_direction_uniform,
        light_color_uniform;

    Int albedo_factor_uniform,
        roughness_factor_uniform,
        metallic_factor_uniform,
        normal_factor_uniform,
        ao_factor_uniform;
};

int main(int argc, char** argv)
{
    if (!glfwInit())
    {
        spdlog::error("Can't initialize GLFW");
        return -1;
    }

    constexpr glm::ivec2 window_size{ 1600, 900 };
    constexpr float aspect = window_size.x / float(window_size.y);
    constexpr float fov = glm::radians(60.0f);
    const glm::mat4 proj = glm::perspective(fov, aspect, 0.01f, 400.0f);

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
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

        GL::Mesh sphere_mesh = MeshTools::compile(Primitives::uvSphereSolid(64, 64, Primitives::UVSphereFlag::Tangents | Primitives::UVSphereFlag::TextureCoordinates), 
            MeshTools::CompileFlag::GenerateSmoothNormals);

        PBRShader pbr_shader; //TODO: pbr_shader -> shader
        //pbr_shader.draw(sphere_mesh);

        Shaders::VertexColor3D shader;
        const glm::mat4 model = glm::scale(glm::mat4(1.0), glm::vec3(50.0f));
        const glm::mat4 view = glm::lookAt(glm::vec3(0.0, 0.0, 100.0), glm::vec3(0.0), glm::vec3(0.0, 1.0, 0.0));
        shader.setTransformationProjectionMatrix((Magnum::Matrix4)(proj * view * model));

        spdlog::info("Initialization successful");

        while (!glfwWindowShouldClose(window)) 
        {
            GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);
            pbr_shader.set_model_matrix(model)
                .set_view_matrix(view)
                .set_proj_matrix(proj)
                .draw(sphere_mesh);
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        glfwTerminate();
    }
    return 0;
}