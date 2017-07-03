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

#pragma once


#include "Components/Transform.h"
#include "Model.h"
#include "Color.h"
#include <DirectXMath.h>

struct ShaderLight
{
	vec3 position;
	float pad1;
	vec3 color;
	float brightness;

	vec3 spotDir;
	float halfAngle;

	XMFLOAT2 attenuation;
	float range;
	float pad3;
};

struct Light// : public Component
{
	friend class Graphics;

	enum class LightType
	{
		POINT = 0,
		SPOT,

		LIGHT_TYPE_COUNT
	};

	Light();
	Light(LightType type, 
			Color color, 
			float range,
			float brightness,
			float spotAngle);
	~Light();

	void SetLightRange(float range);
	XMMATRIX GetLightSpaceMatrix() const;
	XMMATRIX GetViewMatrix() const;
	XMMATRIX GetProjectionMatrix() const;
	ShaderLight ShaderLightStruct() const;

	//---------------------------------------------------------------------------------
	
	// TODO: name consistency | members: m_varName, statics: s_varName
	Transform tf;
	Model model;
	
	LightType lightType_;
	Color color_;
	float range_;
	float brightness_;
	bool shadows_;			// not used yet

	// point light attributes
	XMFLOAT2 attenuation_;

	// spot light attributes
	float spotAngle_;		
};


