#include <GL/glew.h>
#include <GLFW/glfw3.h>


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

#include "QuickGameNetworking/NetAPI.h"
#include "QuickGameNetworking/NetMessages.h"

#define HOST_PORT 8000

DEFINE_EMPTY_NET_DESCRIPTOR_DATA(ControllerDescriptor);

class ObjectDescriptor : public INetObjectDescriptorData
{
	DEFINE_NET_DESCRIPTOR_DATA(ObjectDescriptor);
public:
	ObjectDescriptor() : ObjectDescriptor(0) {}
	ObjectDescriptor(unsigned long id) : m_id(id) {}
	virtual bool operator==(INetObjectDescriptorData const& other)
	{ 
	    return m_id == static_cast<ObjectDescriptor const&>(other).m_id;
	}

private:
	unsigned long m_id;
	friend class boost::serialization::access;
	template<class Archive> void serialize(Archive & ar, const unsigned int version) { ar & m_id; }
};

class ObjectCreationMessage : public NetObjectMessageBase
{
	DEFINE_NET_MESSAGE(ObjectCreationMessage);

public:
	unsigned long int id;

private:
	friend class boost::serialization::access;
	template<class Archive> void serialize(Archive & ar, const unsigned int version) { ar & id; }
};

class ObjectSyncMemento : public INetData
{
	DEFINE_NET_CONTAINER(ObjectSyncMemento);

public:
	float x;
	float y;
	float dx;
	float dy;
	float scale;
	float rot;

private:
	virtual void Serialize(boost::archive::binary_oarchive& stream) const override
	{ 
	    stream << x << y << dx << dy << scale << rot;
	}
	virtual void Deserialize(boost::archive::binary_iarchive& stream) override
	{ 
	    stream >> x >> y >>dx >> dy >> scale >> rot;
	}
};

void server_main() {

}

void client_main() {

}



void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

const char *vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "uniform mat4 transform;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = transform*vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "}\0";
const char *fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
    "}\n\0";

float RandomFloat(float a, float b) {
    float random = ((float) rand()) / (float) RAND_MAX;
    float diff = b - a;
    float r = random * diff;
    return a + r;
}

int main()
{
    std::string input;
    std::cout << "Host?" << std::endl;
    std::getline(std::cin, input);
    bool const isHost = (input.find("y", 0) == 0 || input.find("Y", 0) == 0);

    boost::asio::ip::udp::endpoint hostAddress(boost::asio::ip::address::from_string("127.0.0.1"), HOST_PORT);
    NetObjectAPI::Init(hostAddress, isHost);
    NetDataFactory::GetInstance()->RegisterDataContainer<ControllerDescriptor>();
    NetDataFactory::GetInstance()->RegisterDataContainer<ObjectDescriptor>();
    NetDataFactory::GetInstance()->RegisterDataContainer<ObjectCreationMessage>();
    NetDataFactory::GetInstance()->RegisterDataContainer<ObjectSyncMemento>();
    auto masterNetObj = NetObjectAPI::GetInstance()->CreateThirdPartyNetObject(NetObjectDescriptor::Create<ControllerDescriptor>());
    std::vector<std::unique_ptr<NetObject>> objects;
    std::vector<ObjectSyncMemento*> mementoes;
    auto createObject = [&objects, &mementoes] (ObjectCreationMessage const& message)
    {
	std::cout<<"object created " << message.id <<std::endl; 
	objects.emplace_back(NetObjectAPI::GetInstance()->CreateThirdPartyNetObject(NetObjectDescriptor::Create<ObjectDescriptor>(message.id)));
	mementoes.emplace_back(objects.back()->RegisterMemento<ObjectSyncMemento>());
    };
    masterNetObj->RegisterMessageHandler<ObjectCreationMessage>([createObject](ObjectCreationMessage const& message, NetAddr const& addr){ createObject(message); });

    if (isHost)
    {
        while(objects.size() < 100)
        {
            ObjectCreationMessage msg;
	    msg.id = objects.size();
	    masterNetObj->SendMasterBroadcast(msg, ESendOptions::Reliable);
	    createObject(msg);
	}
    }
        


    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glewExperimental = GL_TRUE;
    glewInit();

    // build and compile our shader program
    // ------------------------------------
    // vertex shader
    int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    // check for shader compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    // fragment shader
    int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    // check for shader compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    // link shaders
    int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    // check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    float vertices[] = {
         0.5f,  0.5f, 0.0f,  // top right
         0.5f, -0.5f, 0.0f,  // bottom right
        -0.5f, -0.5f, 0.0f,  // bottom left
        -0.5f,  0.5f, 0.0f   // top left 
    };
    unsigned int indices[] = {  // note that we start from 0!
        0, 1, 3,  // first Triangle
        1, 2, 3   // second Triangle
    };
    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0); 

    // remember: do NOT unbind the EBO while a VAO is active as the bound element buffer object IS stored in the VAO; keep the EBO bound.
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
    // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
    glBindVertexArray(0); 


    // uncomment this call to draw in wireframe polygons.
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
	std::cout << "objects " << objects.size() <<std::endl;
	NetObjectAPI::GetInstance()->Update();
	std::cout << "updated" <<std::endl;
        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // draw our first triangle
        glUseProgram(shaderProgram);
        glBindVertexArray(VAO); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized

	for (auto& obj : mementoes)
	{
	    if (isHost && (RandomFloat(0, 1) > 0.95f))
	    {
		obj->dx = 0.01f * RandomFloat(-1, 1);
		obj->dy = 0.01f * RandomFloat(-1, 1);
		obj->rot += 0.02f * RandomFloat(-1, 1);
		obj->scale = 0.05f + 0.01f * RandomFloat(-1, 1);
	    }
	    obj->x += obj->dx;
	    obj->y += obj->dy;
	    // create transformations
            glm::mat4 transform = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
	    transform = glm::scale(transform, glm::vec3(obj->scale, obj->scale, obj->scale));
            transform = glm::translate(transform, glm::vec3(obj->x, obj->y, 0.0f));
            transform = glm::rotate(transform, (float)glfwGetTime(), glm::vec3(0.0f, 0.0f, obj->rot));

            // get matrix's uniform location and set matrix
            unsigned int transformLoc = glGetUniformLocation(shaderProgram, "transform");
            glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(transform));

            //glDrawArrays(GL_TRIANGLES, 0, 6);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            // glBindVertexArray(0); // no need to unbind it every time 
        }

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();

    objects.resize(0);
    mementoes.resize(0);
    masterNetObj.reset();
    NetObjectAPI::Shutdown();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}
