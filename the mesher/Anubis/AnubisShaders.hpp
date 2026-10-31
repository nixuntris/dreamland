#pragma once
#include "AnubisMath.hpp"
#include "glad/glad.h"
#include "vector"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <unordered_map>
namespace Anubis {
    struct Color {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    };
    class GPUONLYShortMap {
    public:
        Math::IVector3 size{};
        GLuint textureID = 0;
        GLuint64 handle = 0;

        void Clear(Math::IVector3 s) {
            size = s;
        }
        void Upload(uint16_t* data = nullptr) {
            if (textureID != 0) {
                if (handle != 0) {
                    glMakeImageHandleNonResidentARB(handle);
                    handle = 0;
                }
                glDeleteTextures(1, &textureID);
                textureID = 0;
            }
            glCreateTextures(GL_TEXTURE_3D, 1, &textureID);

            glTextureStorage3D(textureID, 1, GL_R16UI, size.x, size.y, size.z);

            if (data) {
                glTextureSubImage3D(textureID,0,0, 0, 0,size.x, size.y, size.z,GL_RED_INTEGER,GL_UNSIGNED_SHORT,data);
            }
        }
        void UpdateContents(uint16_t* data) {
            if (textureID == 0) {
                Upload(data);
                return;
            }
            glTextureSubImage3D(textureID,0,0, 0, 0,size.x, size.y, size.z,GL_RED_INTEGER,GL_UNSIGNED_SHORT,data);
        }
        GLuint64 GetBindlessHandle(GLenum access = GL_READ_WRITE) {
            if (textureID == 0) {
                Upload();
            }
            if (handle == 0) {
                handle = glGetImageHandleARB(textureID,0, GL_TRUE, 0, GL_R16UI );
                glMakeImageHandleResidentARB(handle, access);
            }

            return handle;
        }
        void Delete() {
            if (handle != 0) {
                glMakeImageHandleNonResidentARB(handle);
                handle = 0;
            }

            if (textureID != 0) {
                glDeleteTextures(1, &textureID);
                textureID = 0;
            }
        }

    };
    class GPUONLYByteMap {
    public:
        Math::IVector3 size{};
        GLuint textureID = 0;
        GLuint64 handle = 0;
        GLuint pboID = 0;

        void Clear(Math::IVector3 s) {
            size = s;
        }
        void Upload(uint8_t* data = nullptr) {

            glCreateTextures(GL_TEXTURE_3D, 1, &textureID);
            glTextureStorage3D(textureID, 1, GL_R8UI, size.x, size.y, size.z);

            if (data) {
                glTextureSubImage3D(textureID, 0, 0, 0, 0,
                    size.x, size.y, size.z,
                    GL_RED_INTEGER, GL_UNSIGNED_BYTE, data);
            }
        }
        void ReuploadContents(uint8_t* data) {
            if (textureID == 0) { Upload(data); return; }
            glTextureSubImage3D(textureID, 0, 0, 0, 0,
                size.x, size.y, size.z,
                GL_RED_INTEGER, GL_UNSIGNED_BYTE, data);
        }
        GLuint64 GetBindlessHandle(GLenum access = GL_READ_WRITE) {
            if (textureID == 0) Upload();
            if (handle == 0) {
                handle = glGetImageHandleARB(textureID, 0, GL_TRUE, 0, GL_R8UI);
                glMakeImageHandleResidentARB(handle, access);
            }
            return handle;
        }
        void Delete() {
            if (handle != 0) {
                glMakeImageHandleNonResidentARB(handle);
                handle = 0;
            }

            if (textureID != 0) {
                glDeleteTextures(1, &textureID);
                textureID = 0;
            }
        }
    };
    class Shader {
    public:
        GLuint programID;
    };
    class ComputeShader : public Shader {
    private:
        unsigned int maxSSBOs;        // Maximum number of SSBOs

        // Load a file's content into a string
        std::string LoadFile(const char* filePath) {
            std::ifstream file(filePath, std::ios::in | std::ios::binary);
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }

        // Check if a shader program linked successfully
        void CheckLinkErrors() {
            GLint success;
            glGetProgramiv(programID, GL_LINK_STATUS, &success);
            if (!success) {
                char infoLog[512];
                glGetProgramInfoLog(programID, 512, nullptr, infoLog);
                std::cerr << "Error: Compute shader program link failed\n" << infoLog << "\n";
            }
        }

        // Check if a shader compiled successfully
        void CheckCompileErrors(GLuint shaderID) {
            GLint success;
            glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);
            if (!success) {
                char infoLog[512];
                glGetShaderInfoLog(shaderID, 512, nullptr, infoLog);
                std::cerr << "Error: Shader compilation failed\n" << infoLog << "\n";
                CheckLinkErrors();
            }
        }

    public:
        std::vector<GLuint> ssboIds;  // Stores SSBO IDs
        GLuint programID;
        // Constructor
        ComputeShader() : programID(0), maxSSBOs(50) {
            ssboIds.resize(maxSSBOs, 0); // Resize vector to hold SSBO IDs
            ssboIds.resize(maxSSBOs, 0); // Resize vector to hold SSBO IDs
        }

        // Destructor - Cleanup all SSBOs
        
        void SetByteMap(GPUONLYByteMap byteMap, int spot, const char* name) {
            glBindImageTexture(spot, byteMap.textureID, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8UI);
            
            glActiveTexture(GL_TEXTURE0 + spot);  // This is not necessary for image binding, but needed for regular texture sampling.
        }
        void UnsetByteMap(int spot) {
            glBindTexture(GL_TEXTURE_3D, 0);
            glBindImageTexture(spot, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R8);
        }
        void Setup(const std::string& path) {
            std::string sourceCode = LoadFile(path.c_str());
            const char* contents = sourceCode.c_str();

            GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
            glShaderSource(computeShader, 1, &contents, nullptr);
            glCompileShader(computeShader);
            CheckCompileErrors(computeShader);

            programID = glCreateProgram();
            glAttachShader(programID, computeShader);
            glLinkProgram(programID);
            CheckLinkErrors();

            glDeleteShader(computeShader);  // Clean up the shader after linking
        }

        // Bind an SSBO at a given index and allocate buffer
        GLuint BindSSBO(int bufferIndex, void* ssboData, size_t size) {
            GLuint id = 0;
            std::cout << bufferIndex << " " << maxSSBOs << "\n";
            if (bufferIndex >= maxSSBOs) {
                std::cerr << "Error: Buffer index out of range\n";
                return id;
            }

            if (ssboIds[bufferIndex] == 0) {
                glGenBuffers(1, &ssboIds[bufferIndex]);
                id = ssboIds[bufferIndex];
            }

            glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboIds[bufferIndex]);
            glBufferData(GL_SHADER_STORAGE_BUFFER, size, ssboData, GL_STATIC_READ);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bufferIndex, ssboIds[bufferIndex]);
            return id;
        }
        void DeleteSSBO(int bufferIndex) {
            if (bufferIndex < 0 || bufferIndex >= maxSSBOs) return;

            if (ssboIds[bufferIndex] != 0) {
                std::cout << "Deleting SSBO " << ssboIds[bufferIndex] << " at index " << bufferIndex << "\n";
                glDeleteBuffers(1, &ssboIds[bufferIndex]);
                ssboIds[bufferIndex] = 0;  // mark as deleted
                std::cout << "ssboIds[" << bufferIndex << "] is now " << ssboIds[bufferIndex] << "\n"; // check it sets to 0
            }
            else {
                std::cout << "SSBO at index " << bufferIndex << " already 0, skipping delete\n";
            }
        }
        void UpdateSSBO(int bufferIndex, void* ssboData, size_t size) {
            if (bufferIndex >= maxSSBOs || ssboIds[bufferIndex] == 0) {
                std::cerr << "Error: Invalid SSBO index or SSBO not created\n";
                return;
            }

            glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboIds[bufferIndex]);

            void* ptr = glMapBufferRange(
                GL_SHADER_STORAGE_BUFFER, 0, size,
                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT
            );

            if (ptr) {
                memcpy(ptr, ssboData, size);
                glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            }
            else {
                std::cerr << "Error: Failed to map SSBO\n";
            }
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bufferIndex, ssboIds[bufferIndex]);
        }

        GLuint BindSSBOGPUONLY(int bufferIndex, size_t size) {
            if (bufferIndex >= maxSSBOs) {
                std::cerr << "Error: Buffer index out of range\n";
                return -1;
            }

            // Create buffer if not already created
            if (ssboIds[bufferIndex] == 0) {
                glGenBuffers(1, &ssboIds[bufferIndex]);
                std::cout << ssboIds[bufferIndex] << " ssbo id \n";
            }

            glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboIds[bufferIndex]);
            glBufferData(GL_SHADER_STORAGE_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bufferIndex, ssboIds[bufferIndex]);
            return ssboIds[bufferIndex];
        }
        // Dispatch the compute shader to run on the GPU
        void Use(int workgroupCountX, int workgroupCountY, int workgroupCountZ) {
            glDispatchCompute(workgroupCountX, workgroupCountY, workgroupCountZ);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
        }

        // Retrieve data from the SSBO (example: read back to CPU)
        void RetrieveData(int bufferIndex, void* data, size_t size) {
            if (bufferIndex >= maxSSBOs) {
                std::cerr << "Error: Buffer index out of range\n";
                return;
            }

            glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboIds[bufferIndex]);
            void* mappedBuffer = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
            if (mappedBuffer) {
                std::memcpy(data, mappedBuffer, size);  // Copy data from buffer
                glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            }
            else {
                std::cerr << "Error: Failed to map SSBO\n";
            }
        }
        void SetVector2(Math::Vector2 vec, const char* name) {
            GLint vec2Location = glGetUniformLocation(programID, name);
            glUniform2f(vec2Location, vec.x, vec.y);
        }
        void SetTextureHandle(uint64_t handle, const std::string& uniformName) {
            GLint location = glGetUniformLocation(programID, uniformName.c_str());
            if (location != -1) {
                glUniformHandleui64ARB(location, handle); // Set the 64-bit texture handle as uniform
            }
            else {
                std::cerr << "Error: Could not find uniform location for " << uniformName << "\n";
            }
        }
        void SetVector3(Math::Vector3 vec, const char* name) {
            GLint vec3Location = glGetUniformLocation(programID, name);
            glUniform3f(vec3Location, vec.x, vec.y, vec.z);
        }
        void SetVector4(Math::Vector4 vec, const char* name) {
            GLint vec4Location = glGetUniformLocation(programID, name);
            glUniform4f(vec4Location, vec.x, vec.y, vec.z, vec.w);
        }
        void SetInt(int integer, const char* name) {
            GLint intLocation = glGetUniformLocation(programID, name);

            glUniform1i(intLocation, integer);
        }
        void SetIntArray(const int* integers, int count, const char* name) {
            GLint intLocation = glGetUniformLocation(programID, name);
            glUniform1iv(intLocation, count, integers);
        }
        void SetUVec2Array(const GLuint64* handles, int count, const char* name) {
            GLint loc = glGetUniformLocation(programID, name);

            for (int i = 0; i < count; ++i) {
                GLuint low = static_cast<GLuint>(handles[i] & 0xFFFFFFFF);
                GLuint high = static_cast<GLuint>(handles[i] >> 32);

                glUniform2ui(loc + i, low, high);
            }
        }
        void SetIntAt(int value, const char* name, int index) {
            GLint loc = glGetUniformLocation(programID, name);
            if (loc != -1) {
                glUniform1i(loc + index, value);
            }
        }
        void SetUVec2Handle(GLuint64 handle, const char* name) {
            GLint loc = glGetUniformLocation(programID, name);
            if (loc != -1) {
                // Use glUniform2ui to split the 64-bit handle into two 32-bit components
                glUniform2ui(loc,
                    static_cast<GLuint>(handle & 0xFFFFFFFF),
                    static_cast<GLuint>(handle >> 32));
            }
        }
        void SetUVec2HandleAt(GLuint64 handle, const char* name, int index) {
            GLint loc = glGetUniformLocation(programID, name);
            if (loc != -1) {
                GLuint low = static_cast<GLuint>(handle & 0xFFFFFFFF);
                GLuint high = static_cast<GLuint>(handle >> 32);
                glUniform2ui(loc + index, low, high);
            }
        }
        void SetUVec2(GLuint64 handle, const char* name) {
            GLuint low = static_cast<GLuint>(handle & 0xFFFFFFFF);
            GLuint high = static_cast<GLuint>(handle >> 32);

            GLint loc = glGetUniformLocation(programID, name);
            glUniform2ui(loc, low, high);
        }
        void SetMatrix(Math::Matrix matrix, const char* name)
        {
            GLint matrixLocation = glGetUniformLocation(programID, name);
            if (matrixLocation != -1)
            {
                glUniformMatrix4fv(
                    matrixLocation,
                    1,
                    GL_FALSE,
                    &matrix.m0
                );
            }
        }
        void SetFloatArray(const float* integers, int count, const char* name) {
            GLint intLocation = glGetUniformLocation(programID, name);
            glUniform1fv(intLocation, count, integers);
        }
        void SetFloat(float floatunit, const char* name) {
            GLint intLocation = glGetUniformLocation(programID, name);

            glUniform1f(intLocation, floatunit);
        }
        void SetVector2Array(Math::Vector2* array, size_t count, const char* name) {
            GLint location = glGetUniformLocation(programID, name);
            GLfloat* flatArray = new GLfloat[count * 2];
            for (size_t i = 0; i < count; ++i) {
                flatArray[i * 2] = array[i].x;
                flatArray[i * 2 + 1] = array[i].y;
            }

            glUniform2fv(location, count, flatArray);

            delete[] flatArray;

        }
        void SetVector3Array(Math::Vector3* array, int count, const char* name) {
            GLint location = glGetUniformLocation(programID, name);
            GLfloat* flatArray = new GLfloat[count * 3];
            for (size_t i = 0; i < count; ++i) {
                flatArray[i * 3] = array[i].x;
                flatArray[i * 3 + 1] = array[i].y;
                flatArray[i * 3 + 2] = array[i].z;
            }

            glUniform3fv(location, count, flatArray);

            delete[] flatArray;
        }
        void SetUIntArray(const unsigned int* uintegers, int count, const char* name) {
            GLint uintLocation = glGetUniformLocation(programID, name);
            glUniform1uiv(uintLocation, count, uintegers);
        }
        void SetVector4Array(Math::Vector4* array, int count, const char* name) {
            GLint location = glGetUniformLocation(programID, name);
            GLfloat* flatArray = new GLfloat[count * 4];
            for (size_t i = 0; i < count; ++i) {
                flatArray[i * 4] = array[i].x;
                flatArray[i * 4 + 1] = array[i].y;
                flatArray[i * 4 + 2] = array[i].z;
                flatArray[i * 4 + 3] = array[i].w;
            }

            glUniform4fv(location, count, flatArray);

            delete[] flatArray;
        }
    };
    void CheckComputeLimits() {
        GLint maxInvocations = 0;
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxInvocations);
        std::cout << "Max total invocations per work group: " << maxInvocations << std::endl;

        GLint maxGroupSizes[3] = { 0, 0, 0 };
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxGroupSizes[0]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &maxGroupSizes[1]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &maxGroupSizes[2]);

        std::cout << "Max individual group sizes (X, Y, Z): "
            << maxGroupSizes[0] << ", "
            << maxGroupSizes[1] << ", "
            << maxGroupSizes[2] << std::endl;
    }

    void CheckComputeShaderLimits() {
        std::cout << "==================================================\n";
        std::cout << "          OPENGL COMPUTE SHADER LIMITS            \n";
        std::cout << "==================================================\n";

        // 1. LOCAL WORK GROUP LIMITS (Size inside the shader)
        GLint maxInvocations = 0;
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxInvocations);
    
        GLint maxLocalSizes[3] = {0, 0, 0};
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxLocalSizes[0]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &maxLocalSizes[1]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &maxLocalSizes[2]);

        std::cout << "[LOCAL WORK GROUP (layout in shader)]\n";
        std::cout << "  -> Max Total Invocations (X * Y * Z): " << maxInvocations << "\n";
        std::cout << "  -> Max Individual Dimensions:\n";
        std::cout << "     X: " << maxLocalSizes[0] << "\n";
        std::cout << "     Y: " << maxLocalSizes[1] << "\n";
        std::cout << "     Z: " << maxLocalSizes[2] << "\n\n";

        // 2. GLOBAL WORK GROUP COUNT LIMITS (Arguments for glDispatchCompute)
        GLint maxGlobalCounts[3] = {0, 0, 0};
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &maxGlobalCounts[0]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &maxGlobalCounts[1]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &maxGlobalCounts[2]);

        std::cout << "[GLOBAL WORK GROUP COUNT (glDispatchCompute arguments)]\n";
        std::cout << "  -> Max Dispatchable Groups:\n";
        std::cout << "     X: " << maxGlobalCounts[0] << "\n";
        std::cout << "     Y: " << maxGlobalCounts[1] << "\n";
        std::cout << "     Z: " << maxGlobalCounts[2] << "\n";
        std::cout << "==================================================\n";
    }
    class DisplayShader : Shader {
    public:
        std::vector<GLuint> ssboIds;
        std::vector<bool> inUsessbo;
    private:
        std::string LoadFile(const char* filePath) {
            std::ifstream file(filePath, std::ios::in | std::ios::binary);
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }
        void Setup(const char* vertex, const char* fragment) {
            inUsessbo.resize(30, false);
            ssboIds.resize(30, 0);
            ssboPointers.resize(30);
            std::cout << "Display shader report: Creating vertex shader \n";

            unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vertexShader, 1, &vertex, nullptr);  // Correct shader source

            glCompileShader(vertexShader);

            // Check for vertex shader compilation errors
            int success;
            glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
            if (!success) {
                int logLength;
                glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &logLength);
                char* infoLog = new char[logLength];
                glGetShaderInfoLog(vertexShader, logLength, nullptr, infoLog);
                std::cerr << "Display shader report: Vertex Shader Compilation Failed\n" << infoLog << std::endl;
                delete[] infoLog;
                return; // Stop execution if there's a compilation error
            }

            std::cout << "Display shader report: Creating fragment shader \n";

            unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fragmentShader, 1, &fragment, nullptr);  // Correct shader source

            glCompileShader(fragmentShader);

            // Check for fragment shader compilation errors
            glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
            if (!success) {
                int logLength;
                glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &logLength);
                char* infoLog = new char[logLength];
                glGetShaderInfoLog(fragmentShader, logLength, nullptr, infoLog);
                std::cerr << "Display shader report: Fragment Shader Compilation Failed\n" << infoLog << std::endl;
                delete[] infoLog;
                return; // Stop execution if there's a compilation error
            }

            std::cout << "Display shader report: Creating complete shader pipeline \n";

            shaderId = glCreateProgram();
            glAttachShader(shaderId, vertexShader);
            glAttachShader(shaderId, fragmentShader);
            glLinkProgram(shaderId);

            // Check for program linking errors
            glGetProgramiv(shaderId, GL_LINK_STATUS, &success);
            if (!success) {
                int logLength;
                glGetProgramiv(shaderId, GL_INFO_LOG_LENGTH, &logLength);
                char* infoLog = new char[logLength];
                glGetProgramInfoLog(shaderId, logLength, nullptr, infoLog);
                std::cerr << "Display shader report: Shader Program Linking Failed\n" << infoLog << std::endl;
                delete[] infoLog;
                return; // Stop execution if linking fails
            }

            GLenum err;
            while ((err = glGetError()) != GL_NO_ERROR) {
                std::cerr << "OpenGL ERROR: " << err << std::endl;
            }

            std::cout << "Display shader report: Deleting shaders \n";
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
        }


    public:
        std::vector<void*> ssboPointers;
        unsigned int shaderId;
        void CompileShaderFromSource(const char* fragment, const char* vertex) {
            Setup(vertex, fragment);
        };

        void LoadBase() {
            Setup("Anubis/BaseShaders/quad_display_shader.vs", "Anubis/BaseShaders/quad_display_shader.fs");
        }
        void CreateSSBO(int bufferIndex, size_t size)
        {
            GLint maxBindings = 0;
            GLint maxBlockSize = 0;

            glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &maxBindings);
            glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &maxBlockSize);
            if (bufferIndex < 0 || bufferIndex >= maxBindings)
            {
                std::cerr << "SSBO ERROR: bufferIndex " << bufferIndex
                    << " exceeds max bindings (" << maxBindings << ")\n";
                return;
            }
            if (size <= 0 || size > (size_t)maxBlockSize)
            {
                std::cerr << "SSBO ERROR: size " << size
                    << " invalid or exceeds max block size (" << maxBlockSize << ")\n";
                return;
            }
            if (bufferIndex >= ssboIds.size() || bufferIndex >= inUsessbo.size())
            {
                std::cerr << "SSBO ERROR: bufferIndex out of bounds for internal arrays\n";
                return;
            }
            GLuint id = 0;
            glGenBuffers(1, &id);

            if (id == 0)
            {
                std::cerr << "SSBO ERROR: glGenBuffers failed\n";
                return;
            }

            ssboIds[bufferIndex] = id;

            glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);

            glBufferData(
                GL_SHADER_STORAGE_BUFFER,
                size,
                nullptr,
                GL_DYNAMIC_DRAW
            );
            GLenum err = glGetError();
            if (err != GL_NO_ERROR)
            {
                std::cerr << "SSBO ERROR: glBufferData failed (error " << err << ")"<<" "<<bufferIndex << "\n";
                glDeleteBuffers(1, &id);
                return;
            }
            glBindBufferBase(
                GL_SHADER_STORAGE_BUFFER,
                bufferIndex,
                id
            );

            err = glGetError();
            if (err != GL_NO_ERROR)
            {
                std::cerr << "SSBO ERROR: glBindBufferBase failed (error " << err << ")\n";
                glDeleteBuffers(1, &id);
                return;
            }

            inUsessbo[bufferIndex] = true;

            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }
        void CreatePersistentSSBO(int bufferIndex, size_t size)
        {
            GLint maxBindings = 0;
            glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &maxBindings);
            if (bufferIndex < 0 || bufferIndex >= maxBindings) return;
            if (bufferIndex >= ssboIds.size() || bufferIndex >= inUsessbo.size()) return;

            GLuint id = 0;
            glGenBuffers(1, &id);
            if (id == 0) return;

            ssboIds[bufferIndex] = id;
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
            glBufferStorage(
                GL_SHADER_STORAGE_BUFFER,
                size,
                nullptr,
                GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT
            );

            void* ptr = glMapBufferRange(
                GL_SHADER_STORAGE_BUFFER,
                0, size,
                GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT
            );
            ssboPointers[bufferIndex] = ptr;

            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bufferIndex, id);
            inUsessbo[bufferIndex] = true;
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }

        void* GetSSBOPointer(int bufferIndex)
        {
            return ssboPointers[bufferIndex];
        }
        void DeleteSSBO(int bufferIndex)
        {
            if (bufferIndex < 0 || bufferIndex >= ssboIds.size() || bufferIndex >= inUsessbo.size())
            {
                std::cerr << "SSBO ERROR: bufferIndex out of bounds\n";
                return;
            }

            GLuint id = ssboIds[bufferIndex];
            if (id != 0)
            {
                glDeleteBuffers(1, &id);
                ssboIds[bufferIndex] = 0;   // Clear stored ID
                inUsessbo[bufferIndex] = false; // Mark as unused
            }
        }
        void ResizeSSBO(int bufferIndex, size_t newSize)
        {
            if (bufferIndex < 0 || bufferIndex >= ssboIds.size() || bufferIndex >= inUsessbo.size())
            {
                std::cerr << "SSBO ERROR: bufferIndex out of bounds\n";
                return;
            }

            GLuint id = ssboIds[bufferIndex];
            if (id == 0)
            {
                std::cerr << "SSBO ERROR: buffer not created yet\n";
                return;
            }

            // Query max block size
            GLint maxBlockSize = 0;
            glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &maxBlockSize);

            if (newSize <= 0 || newSize > (size_t)maxBlockSize)
            {
                std::cerr << "SSBO ERROR: newSize " << newSize
                    << " exceeds max block size (" << maxBlockSize << ")\n";
                return;
            }

            glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);

            // Reallocate buffer with new size (old contents are lost)
            glBufferData(GL_SHADER_STORAGE_BUFFER, newSize, nullptr, GL_DYNAMIC_DRAW);

            GLenum err = glGetError();
            if (err != GL_NO_ERROR)
            {
                std::cerr << "SSBO ERROR: glBufferData (resize) failed (error " << err << ")\n";
                return;
            }

            // Re-bind to slot
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bufferIndex, id);

            err = glGetError();
            if (err != GL_NO_ERROR)
            {
                std::cerr << "SSBO ERROR: glBindBufferBase (resize) failed (error " << err << ")\n";
                return;
            }

            // Optional: unbind
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }
        void UpdateSSBO(int bufferIndex, const void* data, size_t size, size_t startPoint=0)
        {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboIds[bufferIndex]);

            glBufferSubData(
                GL_SHADER_STORAGE_BUFFER,
                startPoint,
                size,
                data
            );
        }
        void SetVector2(Math::Vector2 vec, const char* name) {
            GLint vec2Location = glGetUniformLocation(shaderId, name);
            glUniform2f(vec2Location, vec.x, vec.y);
        }
        void SetVector3(Math::Vector3 vec, const char* name) {
            GLint vec3Location = glGetUniformLocation(shaderId, name);
            glUniform3f(vec3Location, vec.x, vec.y, vec.z);
        }
        void SetMatrix(Math::Matrix matrix, const char* name)
        {
            GLint matrixLocation = glGetUniformLocation(shaderId, name);
            if (matrixLocation != -1)
            {
                glUniformMatrix4fv(
                    matrixLocation,
                    1,
                    GL_FALSE,
                    &matrix.m0
                );
            }
        }

        void SetVector4(Math::Vector4 vec, const char* name) {
            GLint vec4Location = glGetUniformLocation(shaderId, name);
            glUniform4f(vec4Location, vec.x, vec.y, vec.z, vec.w);
        }

        void SetUVec2(GLuint64 handle, const char* name) {
            GLuint low = static_cast<GLuint>(handle & 0xFFFFFFFF);
            GLuint high = static_cast<GLuint>(handle >> 32);

            GLint loc = glGetUniformLocation(shaderId, name);
            glUniform2ui(loc, low, high);
        }
        void SetInt(int integer, const char* name) {
            GLint intLocation = glGetUniformLocation(shaderId, name);
            glUniform1i(intLocation, integer);
        }
        void SetUInt(unsigned int uinteger, const char* name) {
            GLint uintLocation = glGetUniformLocation(shaderId, name);
            glUniform1ui(uintLocation, uinteger);
        }
        void SetFloat(float floatunit, const char* name) {
            GLint intLocation = glGetUniformLocation(shaderId, name);
            glUniform1f(intLocation, floatunit);
        }
        void SetVector2Array(Math::Vector2* array, size_t count, const char* name) {
            GLint location = glGetUniformLocation(shaderId, name);
            GLfloat* flatArray = new GLfloat[count * 2];
            for (size_t i = 0; i < count; ++i) {
                flatArray[i * 2] = array[i].x;
                flatArray[i * 2 + 1] = array[i].y;
            }

            glUniform2fv(location, count, flatArray);

            delete[] flatArray;

        }
        void SetVector3Array(Math::Vector3* array, int count, const char* name) {
            GLint location = glGetUniformLocation(shaderId, name);
            GLfloat* flatArray = new GLfloat[count * 3];
            for (size_t i = 0; i < count; ++i) {
                flatArray[i * 3] = array[i].x;
                flatArray[i * 3 + 1] = array[i].y;
                flatArray[i * 3 + 2] = array[i].z;
            }

            glUniform3fv(location, count, flatArray);

            delete[] flatArray;
        }
        void SetVector4Array(Math::Vector4* array, int count, const char* name) {
            GLint location = glGetUniformLocation(shaderId, name);
            GLfloat* flatArray = new GLfloat[count * 4];
            for (size_t i = 0; i < count; ++i) {
                flatArray[i * 4] = array[i].x;
                flatArray[i * 4 + 1] = array[i].y;
                flatArray[i * 4 + 2] = array[i].z;
                flatArray[i * 4 + 3] = array[i].w;
            }

            glUniform4fv(location, count, flatArray);

            delete[] flatArray;
        }
        void LoadFromFile(const char* fragment, const char* vertex) {
            std::string fragmentShaderCode = LoadFile(fragment);
            std::string vertexShaderCode = LoadFile(vertex);

            const char* vertexShaderSource = vertexShaderCode.c_str();
            const char* fragmentShaderSource = fragmentShaderCode.c_str();
            Setup(vertexShaderSource, fragmentShaderSource);
        }
        void LoadPartialFragment(const char* fragmentSource, const char* vertexPath) {
            std::string vertexShaderCode = LoadFile(vertexPath);

            const char* vertexShaderSource = vertexShaderCode.c_str();
            Setup(vertexShaderSource, fragmentSource);

        }
        void SetIntArray(const int* integers, int count, const char* name) {
            GLint intLocation = glGetUniformLocation(shaderId, name);
            glUniform1iv(intLocation, count, integers);
        }
        void SetUIntArray(const unsigned int* uintegers, int count, const char* name) {
            GLint uintLocation = glGetUniformLocation(shaderId, name);
            glUniform1uiv(uintLocation, count, uintegers);
        }
        void SetFloatArray(const float* integers, int count, const char* name) {
            GLint intLocation = glGetUniformLocation(shaderId, name);
            glUniform1fv(intLocation, count, integers);
        }
        void Use() {
            glUseProgram(shaderId);
        }
    };
    void SwitchShaderStage(GLint programID)
    {
        glUseProgram(programID);
    }
    struct GpuTimer {
        GLuint queryStart, queryEnd;

        GpuTimer() {
            glGenQueries(1, &queryStart);
            glGenQueries(1, &queryEnd);
        }

        ~GpuTimer() {
            glDeleteQueries(1, &queryStart);
            glDeleteQueries(1, &queryEnd);
        }

        inline void Start() {
            glQueryCounter(queryStart, GL_TIMESTAMP);
        }
        inline double Stop() {
            glQueryCounter(queryEnd, GL_TIMESTAMP);

            GLuint64 startTime = 0, endTime = 0;
            GLint done = 0;

            while (!done) {
                glGetQueryObjectiv(queryEnd, GL_QUERY_RESULT_AVAILABLE, &done);
            }

            glGetQueryObjectui64v(queryStart, GL_QUERY_RESULT, &startTime);
            glGetQueryObjectui64v(queryEnd, GL_QUERY_RESULT, &endTime);

            return double(endTime - startTime) / 1e6; // ms
        }
    };
}