#include <Magnum/Magnum.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Platform/GLContext.h>
#include <Magnum/Shaders/VertexColor.h>
#include <GLFW/glfw3.h>
#include <Magnum/GlmIntegration/GtcIntegration.h>
#include <Magnum/GlmIntegration/GtxIntegration.h>
#include <Magnum/GlmIntegration/Integration.h>
#include <spdlog/spdlog.h>

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
        using namespace Magnum;

        Platform::GLContext ctx{ argc, argv };

        GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
        GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);

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