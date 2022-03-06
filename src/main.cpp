#include <Magnum/Magnum.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Mesh.h>
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
    typedef GL::Attribute<0, Vector3> Position;
    typedef GL::Attribute<1, Vector2> TextureCoord;

    explicit PBRShader()
    {

    }

    PBRShader& set_light_direction(const glm::vec3& dir)
    {

        return *this;
    }

    PBRShader& set_light_color(const glm::vec3& color)
    {

        return *this;
    }

    PBRShader& set_albedo_factor(float factor)
    {

        return *this;
    }

    PBRShader& set_roughness_factor(float factor)
    {

        return *this;
    }

    PBRShader& set_metallic_factor(float factor)
    {

        return *this;
    }

    PBRShader& set_normal_factor(float factor)
    {

        return *this;
    }

    PBRShader& set_ao_factor(float factor)
    {

        return *this;
    }

    PBRShader& bind_albedo_texture(GL::Texture2D& tex)
    {

        return *this;
    }

    PBRShader& bind_roughness_texture(GL::Texture2D& tex)
    {

        return *this;
    }

    PBRShader& bind_metallic_texture(GL::Texture2D& tex)
    {

        return *this;
    }

    PBRShader& bind_normal_texture(GL::Texture2D& tex)
    {

        return *this;
    }

    PBRShader& bind_ao_texture(GL::Texture2D& tex)
    {

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

        PBRShader pbr_shader; //TODO: pbr_shader -> shader

        Shaders::VertexColor3D shader;
        const glm::mat4 model = glm::scale(glm::mat4(1.0), glm::vec3(100.0f));
        const glm::mat4 view = glm::lookAt(glm::vec3(0.0, 0.0, 100.0), glm::vec3(0.0), glm::vec3(0.0, 1.0, 0.0));
        shader.setTransformationProjectionMatrix((Magnum::Matrix4)(proj * view * model));

        spdlog::info("Initialization successful");

        while (!glfwWindowShouldClose(window)) 
        {
            GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);
            shader.draw(mesh);
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        glfwTerminate();
    }
    return 0;
}