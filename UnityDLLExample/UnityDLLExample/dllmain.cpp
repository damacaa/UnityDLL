// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "types.h"
#include "Node.h"
#include <math.h>   

bool initialized = false;
bool updated = false;

MyCounter* counter = 0;
Vector3f* vertexArray = 0;
Vector3f* vertexArray2 = 0;
Node* nodeArray = 0;
const int nVertices = 1000;
int nUpdates = 0;
std::mutex vertexMutex;
std::mutex vertexMutex2;

MyCounter* threadCounter = 0;
bool running = false;
std::thread myThread;
float simulationTime = 0;
float delta = 0.01f;


#ifdef __cplusplus
extern "C" {
#endif

	BOOL APIENTRY DllMain(HMODULE hModule,
		DWORD  ul_reason_for_call,
		LPVOID lpReserved
	)
	{
		switch (ul_reason_for_call)
		{
		case DLL_PROCESS_ATTACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
		}
		return TRUE;
	}

	/*Function executed using a separate thread*/
	void StartFunction() {
		while (running) {
			threadCounter->IncreaseCounter();

			std::this_thread::sleep_for(std::chrono::milliseconds(10)); /*Sleep for 10 ms*/
		}
	}

	__declspec(dllexport) void Initialize() {
		if (!initialized) {
			counter = new MyCounter();
			threadCounter = new MyCounter();

			vertexArray = new Vector3f[nVertices];
			vertexArray2 = new Vector3f[nVertices];
			nodeArray = new Node[nVertices];

			int side = sqrt(nVertices);
			int c = 0;
			for (int i = 0; i < side; i++) {
				for (int j = 0; j < side; j++)
				{
					vertexArray[c].x = i;
					vertexArray[c].y = 0.0;
					vertexArray[c].z = j;
					nodeArray[c].position = vertexArray[c];
					c++;
				}
			}

			running = true;
			myThread = std::thread(&StartFunction); /*Create new thread that is executing StartFunction*/
			myThread.detach(); /*Allow thread to run independently*/

			initialized = true;
		}
	}

	__declspec(dllexport) void Destroy() {
		if (initialized) {

			running = false; /*Stop main loop of thread function*/
			if (myThread.joinable()) {
				myThread.join(); /*Wait for thread to finish*/
			}

			delete counter;
			delete threadCounter;
			delete[] vertexArray;
			delete[] vertexArray2;

			counter = 0;
			threadCounter = 0;
			vertexArray = 0;
			vertexArray2 = 0;

			/*Since the variables below are global variables, they exist as long the DLL is loaded. In case of Unity,
			the DLL remains loaded until we close Unity. Therefore we need to reset these variables in order to
			allow Unity to correctly run the simulation again.*/

			initialized = false;
			updated = false;

			nUpdates = 0;

			running = false;
		}
	}

	__declspec(dllexport) void IncreaseCounter() {
		if (counter) {
			counter->IncreaseCounter();
		}
	}

	__declspec(dllexport) int GetCounter() {
		if (counter) {
			return counter->GetCounter();
		}
		return 0;
	}

	__declspec(dllexport) int GetThreadCounter() {
		return threadCounter->GetCounter();
	}

	__declspec(dllexport) void Update(Vector3f position) {
		if (vertexArray) {
			std::lock_guard<std::mutex> lock(vertexMutex); /*Locks mutex and releases mutex once the guard is (implicitly) destroyed*/

			simulationTime += delta;

			/*This may take a long time, depending on your simulation.*/
			for (int i = 0; i < nVertices; i++) {
				nodeArray[i].Update(simulationTime, delta);
				vertexArray[i] = nodeArray[i].position;
			}

			updated = true;

			nUpdates++;
		}
	}

	__declspec(dllexport) Vector3f* GetVertices(int* count) {
		if (count) {
			*count = nVertices;
		}

		/*Depending on how Update is being executed, vertexArray might being updated at the same time. To prevent race conditions, we have to wait
		for the lock of vertexArray and copy all data to a second array. Unity/C# will process the data in the second array. In the meantime
		the data in the first array can be updated.*/

		std::lock_guard<std::mutex> lock(vertexMutex);
		if (updated) {
			std::lock_guard<std::mutex> lock(vertexMutex2);

			memcpy(vertexArray2, vertexArray, sizeof(Vector3f) * nVertices); /*Copy data only if vertexArray has been updated.*/

			updated = false;
		}
		return vertexArray2;
	}
#ifdef __cplusplus
}
#endif


