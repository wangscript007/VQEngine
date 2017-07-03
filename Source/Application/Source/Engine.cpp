//	DX11Renderer - VDemo | DirectX11 Renderer
//	Copyright(C) 2016  - Volkan Ilbeyli
//
//	This program is free software : you can redistribute it and / or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program.If not, see <http://www.gnu.org/licenses/>.
//
//	Contact: volkanilbeyli@gmail.com

#include "Engine.h"

#include "Renderer.h"
#include "Input.h"
#include "PerfTimer.h"
#include "SceneParser.h"
#include "SceneManager.h"

#include "Mesh.h"
#include "Camera.h"

//#include "PhysicsEngine.h"
//#include "../Animation/Include/PathManager.h"

#include <sstream>

Engine* Engine::s_instance = nullptr;

Engine::Engine()
	:
	m_renderer(new Renderer()),
	m_input(new Input()),
	m_scene_manager(new SceneManager()),
	m_timer(new PerfTimer())
{}

Engine::~Engine(){}

bool Engine::Initialize(HWND hWnd, int scr_width, int scr_height)
{
	if (!m_renderer || !m_input || !m_scene_manager || !m_timer) 
		return false;

	m_input->Init();
	if(!m_renderer->Initialize(scr_width, scr_height, hWnd)) 
		return false;
	m_pRenderData = &(m_renderer->m_renderData);
	m_scene_manager->Initialize(m_renderer.get(), m_pRenderData, nullptr);
	return true;
}

bool Engine::Load()
{
	s_SceneParser.ReadScene(m_scene_manager);
	m_timer->Reset();
	return true;
}

void Engine::TogglePause()
{
	m_isPaused = !m_isPaused;
}

void Engine::CalcFrameStats()
{
	static long  frameCount = 0;
	static float timeElaped = -0.9f;
	constexpr float UpdateInterval = 0.5f;

	++frameCount;
	if (m_timer->TotalTime() - timeElaped >= UpdateInterval)
	{
		float fps = static_cast<float>(frameCount);	// #frames / 1.0f
		float frameTime = 1000.0f / fps;			// milliseconds

		std::ostringstream stats;
		stats.precision(2);
		stats	<< "VDemo | "
				<< "dt: " << frameTime << "ms "; 
		stats.precision(4);
		stats	<< "FPS: " << fps;
		SetWindowText(m_renderer->GetWindow(), stats.str().c_str());
		frameCount = 0;
		timeElaped += UpdateInterval;
	}
}


void Engine::Exit()
{
	m_renderer->Exit();

	if (s_instance)
	{
		delete s_instance;
		s_instance = nullptr;
	}
}

shared_ptr<const Input> Engine::INP() const
{
	return m_input;
}

shared_ptr<const PerfTimer>  Engine::TIMER() const
{
	return m_timer;
}

Engine * Engine::GetEngine()
{
	if (s_instance == nullptr)
	{
		s_instance = new Engine();
	}

	return s_instance;

}

bool Engine::Run()
{
	//float begin = m_timer.CurrentTime();	// fps lock
	m_timer->Tick();
	if (m_input->IsKeyDown(VK_ESCAPE))
	{
		return false;
	}

	if (m_input->IsKeyTriggered(0x08)) // Key backspace
		TogglePause(); 

	if (!m_isPaused)
	{
		CalcFrameStats();
		Update(m_timer->DeltaTime());
		Render();
		
		// 70 fps block - won't work because of input lag
		//float end = 0.0f;
		//do 
		//{
		//	end = m_timer.CurrentTime();
		//	//char info[128];
		//	//sprintf_s(info, "end=%f, begin=%f, end-begin=%f\n", end, begin, end - begin);
		//	//OutputDebugString(info);
		//} while ((end-begin) < 0.01428f);		
	}

	m_input->Update();	// update previous state after frame;

#ifdef _DEBUG
	m_renderer->PollShaderFiles();
#endif
	return true;
}

void Engine::Pause()
{
	m_isPaused = true;
}

void Engine::Unpause()
{
	m_isPaused = false;
}

float Engine::TotalTime() const
{
	return m_timer->TotalTime();
}

void Engine::Update(float dt)
{
	//m_physics->Update(dt);
	m_scene_manager->Update(dt);
}

void Engine::Render()
{
	const float clearColor[4] = { 0.5f, 0.8f, 0.5f, 1.0f };
	m_renderer->Begin(clearColor);
	m_renderer->SetViewport(m_renderer->WindowWidth(), m_renderer->WindowHeight());
	m_scene_manager->Render();
	m_renderer->End();
}
