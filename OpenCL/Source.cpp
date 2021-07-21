#include<windows.h>
#include<stdio.h>
#include "MyWindow.h"
#include "vmath.h"

#include<gl/glew.h>
#include<gl/gl.h>

//OpenCL header files
#include<CL/opencl.h>

#pragma comment(lib,"glew32.lib")
#pragma comment(lib,"OpenGL32.lib")
#pragma comment(lib,"OpenCL.lib")

#define WIN_WIDTH 800
#define WIN_HEIGHT 600
#define MY_ARRAY_SIZE mesh_width * mesh_hight * 4

using namespace vmath;

enum {
	AMC_ATTRIBUTE_POSITION = 0,
	AMC_ATTRIBUTE_COLOR,
	AMC_ATTRIBUTE_NORMAL,
	AMC_ATTRIBUTE_TEXTCOORD
};

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

DWORD dwStyle;
HWND ghwnd;
bool gbFullscree = false;
bool gbActiveWindow = false;
WINDOWPLACEMENT wpPrev = { sizeof(WINDOWPLACEMENT) };

FILE* gpFile;

HDC ghdc = NULL;
HGLRC ghrc = NULL;

//shader objects
GLint gVertexShaderObject;
GLint gFragmentShaderObject;
GLint gShaderProgramObject;

//shader binding objects
GLuint vao;
GLuint vboPos;
GLuint mvpMatrixUniform;

//matrix mat4: vmath.h -> typedef : Float16(4 x 4)
mat4 perspectiveProjectionMatrix;

//variables for sin wave
const unsigned int mesh_width = 1024;
const unsigned int mesh_hight = 1024;
float pos[mesh_width][mesh_hight][4];
int arraySize = mesh_width * mesh_hight * 4;

float animationTime = 0.0f;

//variables for OpenCL inter-operability
cl_int clResult;
cl_mem cl_graphic_resource;
cl_device_id oclDeviceId;
cl_context oclContext;
cl_command_queue oclCommandQueue;
cl_program oclProgram;
cl_kernel oclKernel;
//kernel source 
size_t oclKernelSize;
const char* oclKernelSource;
GLuint vboGPU;
bool bOnGPU = false;

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR cmdLine, int iCmdShow) {

	void Initialize(void);
	void Display(void);

	WNDCLASSEX wndclass;
	MSG msg;
	HWND hwnd;
	TCHAR szAppName[] = TEXT("OpenGl Template");
	bool bDone = false;

	if (fopen_s(&gpFile, "MyLog.txt", "w") != 0) {
		MessageBox(NULL, TEXT("Failed to Open file Mylog.txt"), TEXT("ERROR"), MB_OK);
		return (0);
	}

	wndclass.cbSize = sizeof(WNDCLASSEX);
	wndclass.lpszClassName = szAppName;
	wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wndclass.lpfnWndProc = WndProc;
	wndclass.hInstance = hInstance;
	wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));
	wndclass.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.lpszMenuName = NULL;

	RegisterClassEx(&wndclass);

	hwnd = CreateWindowEx(WS_EX_APPWINDOW,
		szAppName,
		TEXT("Sin-Wave."),
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE,
		100,
		100,
		WIN_WIDTH,
		WIN_HEIGHT,
		0,
		0,
		hInstance,
		NULL);

	if (hwnd == NULL) {
		MessageBox(NULL, TEXT("Failed to Create Window."), TEXT("ERROR!"), MB_OK);
		exit(0);
	}
	ghwnd = hwnd;

	Initialize();
	ShowWindow(hwnd, iCmdShow);

	SetForegroundWindow(hwnd);
	SetFocus(hwnd);

	while (bDone == false) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				bDone = true;
			}
			else {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else {
			if (gbActiveWindow == true) {
				Display();
			}
		}
	}

	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam) {

	void Resize(int, int);
	void UnInitialize(void);
	void ToggleFullscreen(void);

	MONITORINFO mi = { sizeof(MONITORINFO) };

	switch (iMsg) {
	case WM_SETFOCUS:
		gbActiveWindow = true;
		break;
	case WM_KILLFOCUS:
		gbActiveWindow = false;
		break;
	case WM_SIZE:
		Resize(LOWORD(lParam), HIWORD(lParam));
		break;
	case WM_ERASEBKGND:
		return(0);
	case WM_KEYDOWN:
		switch (wParam) {
		case VK_ESCAPE:DestroyWindow(ghwnd);
			break;
		case 0x46:
		case 0x66:
			ToggleFullscreen();
			break;
		case 'C':
		case 'c':
			bOnGPU = false;
			break;
		case 'G':
		case 'g':
			bOnGPU = true;
			break;
		default:
			break;
		}
		break;
	case WM_DESTROY:
		UnInitialize();
		PostQuitMessage(0);
		break;
	default:break;
	}

	return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

void ToggleFullscreen(void) {
	MONITORINFO mi = { sizeof(MONITORINFO) };
	if (gbFullscree == false) {
		dwStyle = GetWindowLong(ghwnd, GWL_STYLE);
		if (dwStyle & WS_OVERLAPPEDWINDOW) {
			if (GetWindowPlacement(ghwnd, &wpPrev) && GetMonitorInfo(MonitorFromWindow(ghwnd, MONITORINFOF_PRIMARY), &mi)) {
				SetWindowLong(ghwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
				SetWindowPos(ghwnd,
					HWND_TOP,
					mi.rcMonitor.left,
					mi.rcMonitor.top,
					mi.rcMonitor.right - mi.rcMonitor.left,
					mi.rcMonitor.bottom - mi.rcMonitor.top,
					SWP_NOZORDER | SWP_FRAMECHANGED);
			}
		}
		ShowCursor(FALSE);
		gbFullscree = true;
	}
	else {
		ShowCursor(TRUE);
		SetWindowLong(ghwnd, GWL_STYLE, dwStyle);
		SetWindowPlacement(ghwnd, &wpPrev);
		SetWindowPos(ghwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
		gbFullscree = false;


	}
}

void Initialize(void) {
	void Resize(int, int);
	char * LoadoclKernelSource(const char * filename,const char * preamble, size_t *sizeFinalLength);

	PIXELFORMATDESCRIPTOR pfd;
	int iPixelFormatIndex = 0;

	//OpenCL context needs OpenGL context
	cl_platform_id oclPlatformId;
	cl_device_id* arrayDeviceIds;
	cl_uint devCount;

	ghdc = GetDC(ghwnd);

	ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cRedBits = 8;
	pfd.cGreenBits = 8;
	pfd.cBlueBits = 8;
	pfd.cAlphaBits = 8;
	pfd.cDepthBits = 32;

	iPixelFormatIndex = ChoosePixelFormat(ghdc, &pfd);
	if (iPixelFormatIndex == 0) {
		fprintf(gpFile, "ChoosePixelFormat() Failed\n");
		DestroyWindow(ghwnd);
	}
	if (SetPixelFormat(ghdc, iPixelFormatIndex, &pfd) == FALSE) {
		fprintf(gpFile, "SetPixelFormat() Failed\n");
		DestroyWindow(ghwnd);
	}
	ghrc = wglCreateContext(ghdc);
	if (ghrc == NULL) {
		fprintf(gpFile, "wglCreateContext() Failed\n");
		DestroyWindow(ghwnd);
	}
	if (wglMakeCurrent(ghdc, ghrc) == FALSE) {
		fprintf(gpFile, "wglMakeCurrent() Failed\n");
		DestroyWindow(ghwnd);
	}
	//Glew initilalization code
	GLenum glew_error = glewInit();
	if (glew_error != GLEW_OK) {
		wglDeleteContext(ghrc);
		ghrc = NULL;
		ReleaseDC(ghwnd, ghdc);
		ghdc = NULL;
	}

	//OpenGL realted logs
	fprintf(gpFile, "\n\nOpenGL vendor : %s \n", glGetString(GL_VENDOR));
	fprintf(gpFile, "OpenGL renderer : %s \n", glGetString(GL_RENDERER));
	fprintf(gpFile, "OpenGL renderer : %s \n", glGetString(GL_RENDERER));
	fprintf(gpFile, "OpenGL version : %s \n", glGetString(GL_VERSION));
	fprintf(gpFile, "GLSL version : %s \n\n ", glGetString(GL_SHADING_LANGUAGE_VERSION));

	//OpenGL enabled extensions
	GLint numExt;
	glGetIntegerv(GL_NUM_EXTENSIONS, &numExt);
	fprintf(gpFile, "\n\nPrinting extensions\n\n");

	//loop
	for (int i = 0; i < numExt; i++) {
		fprintf(gpFile, "%s \n", glGetStringi(GL_EXTENSIONS, i));
	}

	fprintf(gpFile, "\n\nStarted OpenCl context creation\n\n");

	
	clResult = clGetPlatformIDs(1, &oclPlatformId, NULL);
	if (clResult != CL_SUCCESS) {
		//error checking
		fprintf(gpFile, "Failed to get platform ids\n");
		DestroyWindow(ghwnd);
	}
	fprintf(gpFile, "got the platform id\n");

	clResult= clGetDeviceIDs(oclPlatformId,CL_DEVICE_TYPE_GPU,0,NULL, &devCount);
	if (clResult != CL_SUCCESS) {
		//error checking
		fprintf(gpFile, "Failed to get device ids\n");
		DestroyWindow(ghwnd);
	}
	else if (devCount == 0) {
		//error checking
		fprintf(gpFile, "No device found\n");
		DestroyWindow(ghwnd);
	}
	else {
		fprintf(gpFile, "got the device  id\n");
		arrayDeviceIds = (cl_device_id*)malloc(sizeof(cl_device_id) * devCount);
		if (arrayDeviceIds == NULL) {
			//error checking
			fprintf(gpFile, "Memory allocation failed for arrayDeviceIds\n");
			DestroyWindow(ghwnd);
		}
		fprintf(gpFile, "memory allocated for arrayDeviceIds\n");
		
		clResult = clGetDeviceIDs(oclPlatformId, CL_DEVICE_TYPE_GPU, devCount, arrayDeviceIds, NULL);
		if (clResult != CL_SUCCESS) {
			//error checking
			fprintf(gpFile, "No device found\n");
			DestroyWindow(ghwnd);
		}

		oclDeviceId = arrayDeviceIds[0];

		fprintf(gpFile, "Device ID setting done \n");

		free(arrayDeviceIds);
		arrayDeviceIds = NULL;
	}

	//array of key value pair of properties
	cl_context_properties arrayContextProperties[] = {
		CL_GL_CONTEXT_KHR,(cl_context_properties)wglGetCurrentContext(),
		CL_WGL_HDC_KHR , (cl_context_properties)wglGetCurrentDC(),
		CL_CONTEXT_PLATFORM,(cl_context_properties)oclPlatformId,
		0
	};

	//5 : create global context
	oclContext = clCreateContext(arrayContextProperties,1,&oclDeviceId,NULL,NULL,&clResult);
	if (clResult != CL_SUCCESS) {
		//error checking
		fprintf(gpFile, "clCreateContext failed\n");
		DestroyWindow(ghwnd);
	}
	fprintf(gpFile, "OpenCL context created successfully\n");

	//create command queue
	oclCommandQueue =  clCreateCommandQueueWithProperties(oclContext,oclDeviceId,0,&clResult);
	if (clResult != CL_SUCCESS) {
		//error checking
		fprintf(gpFile, "clCreateCommandQueueWithProperties Failed\n");
		DestroyWindow(ghwnd);
	}
	fprintf(gpFile, "Command queue created successfully\n");

	oclKernelSource = LoadoclKernelSource("sinewave.cl","",&oclKernelSize);
	fprintf(gpFile, "Kernal source code loading done\n");

	oclProgram = clCreateProgramWithSource(oclContext,devCount,&oclKernelSource,&oclKernelSize,&clResult);
	if (clResult != CL_SUCCESS) {
		//error checking
		fprintf(gpFile, "clCreateCommandQueueWithProperties Failed\n");
		DestroyWindow(ghwnd);
	}
	fprintf(gpFile, "OCL program created\n");

	clResult = clBuildProgram(oclProgram,0,NULL,"-cl-fast-relaxed-math", NULL, NULL); // ? @1:10:00
	if (clResult != CL_SUCCESS) {
		//error checking
		fprintf(gpFile, "OpenCL Error - clBuildProgram() Failed : %d. Exiting now...\n", clResult);
		size_t len;
		char buffer[2048];
		clGetProgramBuildInfo(oclProgram, oclDeviceId,CL_PROGRAM_BUILD_LOG,sizeof(buffer),buffer,&len);
		printf("OpenGL Program build logs : %s \n",buffer);
		DestroyWindow(ghwnd);
	}
	fprintf(gpFile, "program built!\n");

	//9
	oclKernel =	clCreateKernel(oclProgram,"sinewave_kernel",&clResult);
	if (clResult != CL_SUCCESS) {
		//error checking
		fprintf(gpFile, "clCreateKernel Failed\n");
		DestroyWindow(ghwnd);
	}
	fprintf(gpFile, "clCreateKernel Done!\n");
	//fill graphic resource
	
	//Vertex shader
	//create shader
	gVertexShaderObject = glCreateShader(GL_VERTEX_SHADER);

	//provide source object to shader
	/*const GLchar* vertexShaderSourceCode =
		"#version 450"\
		"\n"\
		"void main(void)"\
		"{"\
		"}";*/
	const GLchar* vertexShaderSourceCode =
		"#version 440 core "\
		"\n"\
		"in vec4 vPosition;"\
		"uniform mat4 u_mvpMatrix;"\
		"void main(void)"\
		"{"\
		"gl_Position = u_mvpMatrix * vPosition;"\
		"}";

	glShaderSource(gVertexShaderObject, 1, (const GLchar**)&vertexShaderSourceCode, NULL);

	//compile shader
	glCompileShader(gVertexShaderObject);

	//shader compilation error checking here
	GLint infoLogLength = 0;
	GLint shaderCompilationStatus = 0;
	char* szBuffer = NULL;
	glGetShaderiv(gVertexShaderObject, GL_COMPILE_STATUS, &shaderCompilationStatus);
	if (shaderCompilationStatus == GL_FALSE) {
		glGetShaderiv(gVertexShaderObject, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 0) {
			szBuffer = (char*)malloc(infoLogLength);
			if (szBuffer != NULL) {
				GLint written;
				glGetShaderInfoLog(gVertexShaderObject, infoLogLength, &written, szBuffer);
				//print log to file
				fprintf(gpFile, "Vertex shader logs : %s \n", szBuffer);
				free(szBuffer);
				DestroyWindow(ghwnd);
				//UnInitialize();
				//exit(0);
			}
		}
	}

	//fragment shader
	//create shader
	gFragmentShaderObject = glCreateShader(GL_FRAGMENT_SHADER);

	//provide source object to shader
	const GLchar* fragmentShaderSourceCode =
		"#version 440 core "\
		"\n"\
		"out vec4 FragColor;"\
		"void main(void)"\
		"{"\
		"	FragColor = vec4(1.0,0.7,0.0,1.0);"\
		"}";

	glShaderSource(gFragmentShaderObject, 1, (const GLchar**)&fragmentShaderSourceCode, NULL);

	//compile shader
	glCompileShader(gFragmentShaderObject);
	//shader compilation error checking here
	infoLogLength = 0;
	shaderCompilationStatus = 0;
	szBuffer = NULL;
	glGetShaderiv(gFragmentShaderObject, GL_COMPILE_STATUS, &shaderCompilationStatus);
	if (shaderCompilationStatus == GL_FALSE) {
		glGetShaderiv(gFragmentShaderObject, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 0) {
			szBuffer = (char*)malloc(infoLogLength);
			if (szBuffer != NULL) {
				GLint written;
				glGetShaderInfoLog(gFragmentShaderObject, infoLogLength, &written, szBuffer);
				//print log to file
				fprintf(gpFile, "Fragment shader logs : %s \n", szBuffer);
				free(szBuffer);
				DestroyWindow(ghwnd);
			}
		}
	}

	//Shader program
	//create
	gShaderProgramObject = glCreateProgram();

	//attach vertext shader to shader program
	glAttachShader(gShaderProgramObject, gVertexShaderObject);
	//attach fragment shader to shader program 
	glAttachShader(gShaderProgramObject, gFragmentShaderObject);

	//bind attribute with the one which we have specified with in in vertex shader
	glBindAttribLocation(gShaderProgramObject, AMC_ATTRIBUTE_POSITION, "vPosition");

	//link shader
	glLinkProgram(gShaderProgramObject);
	//linking error cheking code
	GLint shaderProgramLinkStatus = 0;
	szBuffer = NULL;
	glGetProgramiv(gShaderProgramObject, GL_LINK_STATUS, &shaderProgramLinkStatus);
	if (shaderProgramLinkStatus == GL_FALSE) {
		glGetProgramiv(gShaderProgramObject, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > 0) {
			szBuffer = (char*)malloc(infoLogLength);
			if (szBuffer != NULL) {
				GLint written;
				glGetProgramInfoLog(gShaderProgramObject, infoLogLength, &written, szBuffer);
				//print log to file
				fprintf(gpFile, "Fragment shader logs : %s \n", szBuffer);
				free(szBuffer);
				DestroyWindow(ghwnd);
			}
		}
	}

	//get MVP uniform location 
	mvpMatrixUniform = glGetUniformLocation(gShaderProgramObject, "u_mvpMatrix");

	//initialize pos array
	for (int i = 0; i < mesh_width; i++) {
		for (int j = 0; j < mesh_hight; j++) {
			for (int k = 0; k < 4; k++) {
				pos[i][j][k] = 0.0f;
			}
		}
	}

	glGenVertexArrays(1, &vao); //?
	glBindVertexArray(vao); //?

	//push this array to shader
	glGenBuffers(1, &vboPos);
	glBindBuffer(GL_ARRAY_BUFFER, vboPos);
	glBufferData(GL_ARRAY_BUFFER, MY_ARRAY_SIZE * sizeof(float), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glGenBuffers(1, &vboGPU);
	glBindBuffer(GL_ARRAY_BUFFER, vboGPU);
	glBufferData(GL_ARRAY_BUFFER, MY_ARRAY_SIZE * sizeof(float), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	//create vboGPU
	cl_graphic_resource = clCreateFromGLBuffer(oclContext,CL_MEM_WRITE_ONLY,vboGPU,&clResult);
	if (clResult != CL_SUCCESS) {
		//error checking
		fprintf(gpFile, "clCreateFromGLBuffer Failed\n");
		DestroyWindow(ghwnd);
	}

	glShadeModel(GL_SMOOTH);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glEnable(GL_CULL_FACE);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	perspectiveProjectionMatrix = mat4::identity();
	//warmup resize
	Resize(WIN_WIDTH, WIN_HEIGHT);

}

char* LoadoclKernelSource(const char* filename, const char* preamble, size_t* sizeFinalLength) {
	//locals
	FILE* pFile = NULL;
	size_t sizeSourceLength;


	if (fopen_s(&pFile, filename, "rb") != 0) {
		MessageBox(NULL, TEXT("Failed to Open file Mylog.txt"), TEXT("ERROR"), MB_OK);
		return (0);
	}
	//pFile = fopen(filename,"rb"); //binary read
	//if (pFile == NULL) {
	//	return NULL;
	//}

	size_t sizePreambleLength = (size_t)strlen(preamble);

	//get the length of source code
	fseek(pFile,0,SEEK_END);
	sizeSourceLength = ftell(pFile);
	fseek(pFile,0,SEEK_SET);

	//allocate a buffer for source code 
	char* sourceString = (char*)malloc(sizeSourceLength + sizePreambleLength +1);
	memcpy(sourceString,preamble,sizePreambleLength);
	if (fread((sourceString)+sizePreambleLength,sizeSourceLength,1,pFile)!=1) {
		fclose(pFile);
		free(sourceString);
		return(0);
	}
	fclose(pFile);
	if (sizeFinalLength != 0) {
		*sizeFinalLength = sizeSourceLength + sizePreambleLength;
	}
	sourceString[sizeSourceLength + sizePreambleLength] = '\0';

	return(sourceString);
}

void Resize(int width, int height) {
	if (height == 0)
		height = 1;
	glViewport(0, 0, (GLsizei)width, (GLsizei)height);

	//gluPerspective(45.0f, (GLfloat)width / (GLfloat)height, 0.1f, 100.0f);
	perspectiveProjectionMatrix = vmath::perspective(45.0f, (GLfloat)width / (GLfloat)height, 0.1f, 100.0f);
}

void Display(void) {
	void UnInitialize(void);
	void LaunchCPUKernel(unsigned int mesh_width, unsigned int mesh_hight, float animationTime);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//start using opengl program object
	glUseProgram(gShaderProgramObject);

	//OpenGL drawing
	//set modelview and projection matrix to dentity 
	mat4 modelViewMatrix = mat4::identity();
	mat4 modelViewprojectionMatrix = mat4::identity();
	modelViewprojectionMatrix = perspectiveProjectionMatrix * modelViewMatrix;
	glUniformMatrix4fv(mvpMatrixUniform, 1, GL_FALSE, modelViewprojectionMatrix);

	glBindVertexArray(vao); //bind vao

	if (bOnGPU == true) {
		
		//cl_graphic_resource vboGPU /POS
		clResult = clSetKernelArg(oclKernel,0,sizeof(cl_mem),&cl_graphic_resource);
		if (clResult != CL_SUCCESS) {
			fprintf(gpFile,"OpenCL error clSetKernelArg failed for cl_graphic_resource");
			UnInitialize();
		}
		//meshwidth 
		clResult = clSetKernelArg(oclKernel, 1, sizeof(cl_int), &mesh_width);
		if (clResult != CL_SUCCESS) {
			fprintf(gpFile, "OpenCL error clSetKernelArg failed for mesh_width");
			UnInitialize();
		}
		//mesh height
		clResult = clSetKernelArg(oclKernel, 2, sizeof(cl_int), &mesh_hight);
		if (clResult != CL_SUCCESS) {
			fprintf(gpFile, "OpenCL error clSetKernelArg failed for mesh_hight");
			UnInitialize();
		}
		//time
		clResult = clSetKernelArg(oclKernel, 3, sizeof(cl_float),&animationTime);
		if (clResult != CL_SUCCESS) {
			fprintf(gpFile, "OpenCL error clSetKernelArg failed for animationTime");
			UnInitialize();
		}

		clResult =  clEnqueueAcquireGLObjects(oclCommandQueue,1,&cl_graphic_resource,0,NULL,NULL);
		if (clResult != CL_SUCCESS) {
			fprintf(gpFile, "OpenCL error clEnqueueAcquireGLObjects");
			UnInitialize();
		}
		fprintf(gpFile, "enqueue done");

		size_t globalWorkSize[2];
		globalWorkSize[0] = mesh_width;
		globalWorkSize[1] = mesh_hight;
		clResult = clEnqueueNDRangeKernel(oclCommandQueue,oclKernel,2,NULL,globalWorkSize, 0,0,NULL,NULL);
		if (clResult != CL_SUCCESS) {
			fprintf(gpFile, "OpenCL error clEnqueueNDRangeKernel");
			UnInitialize();
		}

		clResult = clEnqueueReleaseGLObjects(oclCommandQueue, 1, &cl_graphic_resource, 0, NULL, NULL);
		if (clResult != CL_SUCCESS) {
			fprintf(gpFile, "OpenCL error clEnqueueReleaseGLObjects");
			UnInitialize();
		}
		fprintf(gpFile, "dequeue done");

		clFinish(oclCommandQueue);

		glBindBuffer(GL_ARRAY_BUFFER, vboGPU);
	}
	else {
		LaunchCPUKernel(mesh_width, mesh_hight, animationTime);

		glBindBuffer(GL_ARRAY_BUFFER, vboPos);
		glBufferData(GL_ARRAY_BUFFER, MY_ARRAY_SIZE * sizeof(float), pos, GL_DYNAMIC_DRAW);
	}
	glVertexAttribPointer(AMC_ATTRIBUTE_POSITION, 4, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(AMC_ATTRIBUTE_POSITION);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glDrawArrays(GL_POINTS, 0, mesh_width * mesh_hight);
	glBindVertexArray(0); //unbind vao
	animationTime = animationTime + 0.01f;
	//stop using program
	glUseProgram(0);

	//glFlush();
	SwapBuffers(ghdc);
}

void LaunchCPUKernel(unsigned int mesh_width, unsigned int mesh_hight, float time) {
	for (int i = 0; i < mesh_width; i++) {
		for (int j = 0; j < mesh_hight; j++) {
			for (int k = 0; k < 4; k++) {
				float u = i / (float)mesh_width;
				float v = j / (float)mesh_hight;
				u = u * 2.0f - 1.0f;
				v = v * 2.0f - 1.0f;
				float freq = 4.0f;
				float w = sinf(u * freq + time) * cosf(v * freq + time) * 0.5f;
				if (k == 0) {
					pos[i][j][k] = u;
				}
				else if (k == 1) {
					pos[i][j][k] = w;
				}
				else if (k == 2) {
					pos[i][j][k] = v;
				}
				else if (k == 3) {
					pos[i][j][k] = 1.0f;
				}
			}
		}
	}
}


void UnInitialize(void) {
	dwStyle = GetWindowLong(ghwnd, GWL_STYLE);
	ShowCursor(TRUE);
	SetWindowLong(ghwnd, GWL_STYLE, dwStyle);
	SetWindowPlacement(ghwnd, &wpPrev);
	SetWindowPos(ghwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

	if (vao) {
		glDeleteVertexArrays(1, &vao);
		vao = 0;
	}

	if (vboPos) {
		glDeleteBuffers(1, &vboPos);
		vboPos = 0;
	}

	if (vboGPU) {
		glDeleteBuffers(1,&vboGPU);
		vboGPU = 0;
	}
	if (oclDeviceId) {
		free(oclDeviceId);
	}
	
	clReleaseMemObject(cl_graphic_resource);

	cl_graphic_resource = NULL;

	if (oclKernel) {
		clReleaseKernel(oclKernel);
		oclKernel = NULL;
	}
	if (oclProgram) {
		clReleaseProgram(oclProgram);
		oclProgram = NULL;
	}
	if (oclCommandQueue) {
		clReleaseCommandQueue(oclCommandQueue);
		oclCommandQueue = NULL;
	}
	if (oclContext) {
		clReleaseContext(oclContext);
		oclContext = NULL;
	}
	
	//safe release changes
	if (gShaderProgramObject) {
		glUseProgram(gShaderProgramObject);
		//shader cound to shaders attached to shader prog object
		GLsizei shaderCount;
		glGetProgramiv(gShaderProgramObject, GL_ATTACHED_SHADERS, &shaderCount);
		GLuint* pShaders;
		pShaders = (GLuint*)malloc(sizeof(GLuint) * shaderCount);
		if (pShaders == NULL) {
			fprintf(gpFile, "Failed to allocate memory for pShaders");
			return;
		}
		//1st shader count is expected value we are passing and 2nd variable we are passing address in which
		//we are getting actual shader count currently attached to shader prog 
		glGetAttachedShaders(gShaderProgramObject, shaderCount, &shaderCount, pShaders);
		for (GLsizei i = 0; i < shaderCount; i++) {
			glDetachShader(gShaderProgramObject, pShaders[i]);
			glDeleteShader(pShaders[i]);
			pShaders[i] = 0;
		}
		free(pShaders);
		glDeleteProgram(gShaderProgramObject);
		gShaderProgramObject = 0;
		glUseProgram(0);
	}

	/*glDetachShader(gShaderProgramObject , gVertexShaderObject);
	glDetachShader(gShaderProgramObject, gFragmentShaderObject);
	glDeleteShader(gVertexShaderObject);
	gVertexShaderObject = 0;
	glDeleteShader(gFragmentShaderObject);
	gFragmentShaderObject = 0;
	glDeleteShader(gShaderProgramObject);
	gShaderProgramObject = 0;
	glUseProgram(0);
	*/

	if (wglGetCurrentContext() == ghrc) {
		wglMakeCurrent(NULL, NULL);
	}
	if (ghrc) {
		wglDeleteContext(ghrc);
		ghrc = NULL;
	}
	if (ghdc) {
		ReleaseDC(ghwnd, ghdc);
		ghdc = NULL;
	}
	if (gpFile) {
		fclose(gpFile);
		gpFile = NULL;
	}
}
