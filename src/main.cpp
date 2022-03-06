#include <Magnum/Magnum.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Platform/GLContext.h>
#include <Magnum/Shaders/VertexColor.h>
#include <GLFW/glfw3.h>

int main(int argc, char** argv)
{
    if (!glfwInit())
    {
        return -1;
    }

    GLFWwindow* const window = glfwCreateWindow(1280, 720, "Demo", nullptr, nullptr);

    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    {
        using namespace Magnum;

        Platform::GLContext ctx{ argc, argv };

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

        while (!glfwWindowShouldClose(window)) 
        {
            GL::defaultFramebuffer.clear(GL::FramebufferClear::Color);
            shader.draw(mesh);
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        glfwTerminate();
    }
    return 0;
}