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

#include "Shader.h"
#include "Log.h"
#include <fstream>
#include <sstream>


CPUConstant::CPUConstantPool CPUConstant::s_constants;
size_t CPUConstant::s_nextConstIndex = 0;

// HELPER FUNCTIONS & CONSTANTS
// ============================================================================
void OutputShaderErrorMessage(ID3D10Blob* errorMessage, const CHAR* shaderFileName)
{
	char* compileErrors = (char*)errorMessage->GetBufferPointer();
	size_t bufferSize = errorMessage->GetBufferSize();

	std::stringstream ss;
	for (unsigned int i = 0; i < bufferSize; ++i)
	{
		ss << compileErrors[i];
	}
	OutputDebugString(ss.str().c_str());

	errorMessage->Release();
	errorMessage = 0;
	return;
}

void HandleCompileError(ID3D10Blob* errorMessage, const std::string& shdPath)
{
	if (errorMessage)
	{
		OutputShaderErrorMessage(errorMessage, shdPath.c_str());
	}
	else
	{
		Log::Error(ERROR_LOG::CANT_OPEN_FILE, shdPath);
	}

	// continue execution, make sure error is known
	//assert(false);
}

static void(__cdecl ID3D11DeviceContext:: *SetShaderConstants[6])
(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) = {
	&ID3D11DeviceContext::VSSetConstantBuffers,
	&ID3D11DeviceContext::GSSetConstantBuffers,
	&ID3D11DeviceContext::DSSetConstantBuffers,
	&ID3D11DeviceContext::HSSetConstantBuffers,
	&ID3D11DeviceContext::CSSetConstantBuffers,
	&ID3D11DeviceContext::PSSetConstantBuffers };
// ============================================================================

std::array<ShaderID, SHADERS::SHADER_COUNT> Shader::s_shaders;

Shader::Shader(const std::string& shaderFileName)
	:
	m_vertexShader(nullptr),
	m_pixelShader(nullptr),
	m_geometryShader(nullptr),
	m_layout(nullptr),
	m_name(shaderFileName),
	m_id(-1)
{}

Shader::~Shader(void)
{
#if _DEBUG 
	//Log::Info("Shader dtor: %s", m_name.c_str());
#endif

	// release constants
	for (ConstantBuffer& cbuf : m_cBuffers)
	{
		if (cbuf.data)
		{
			cbuf.data->Release();
			cbuf.data = nullptr;
		}
	}

	if (m_layout)
	{
		m_layout->Release();
		m_layout = nullptr;
	}

	if (m_pixelShader)
	{
		m_pixelShader->Release();
		m_pixelShader = nullptr;
	}

	if (m_vertexShader)
	{
		m_vertexShader->Release();
		m_vertexShader = nullptr;
	}

	if (m_geometryShader)
	{
		m_geometryShader->Release();
		m_geometryShader = nullptr;
	}


	if (m_vsRefl)
	{
		m_vsRefl->Release();
		m_vsRefl = nullptr;
	}

	if (m_psRefl)
	{
		m_psRefl->Release();
		m_psRefl = nullptr;
	}
}

void Shader::Compile(ID3D11Device* device, const std::string& shaderFileName, const std::vector<InputLayout>& layouts, bool geoShader)
{
	HRESULT result;
	ID3D10Blob* errorMessage = nullptr;

	// SHADER PATHS
	//----------------------------------------------------------------------------
	std::string vspath = shaderFileName + "_vs.hlsl";
	std::string gspath = shaderFileName + "_gs.hlsl";
	std::string pspath = shaderFileName + "_ps.hlsl";
	std::wstring vspath_w(vspath.begin(), vspath.end());
	std::wstring gspath_w(gspath.begin(), gspath.end());
	std::wstring pspath_w(pspath.begin(), pspath.end());
	const WCHAR* VSPath = vspath_w.c_str();
	const WCHAR* GSPath = gspath_w.c_str();
	const WCHAR* PSPath = pspath_w.c_str();

	std::string info("\tCompiling  \""); info += m_name; info += "\"...\t";
	Log::Info(info);

	// COMPILE SHADERS
	//----------------------------------------------------------------------------
	ID3DInclude* const	includeHandler				= D3D_COMPILE_STANDARD_FILE_INCLUDE;		// use default include handler for using #include in shader files
	const char*			shaderCompilerVersions[]	= { "vs_5_0", "gs_5_0", "", "", "", "ps_5_0" };

#if defined( _DEBUG ) || defined ( FORCE_DEBUG )
	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#endif

	// vertex shader
	ID3D10Blob* vsBlob = NULL;
	if (FAILED(D3DCompileFromFile(
		VSPath,
		NULL,
		includeHandler ,
		"VSMain",
		shaderCompilerVersions[ShaderType::VS],
		flags,
		0,
		&vsBlob,
		&errorMessage)))
	{
		HandleCompileError(errorMessage, vspath);
		return;
	}

	// geometry shader
	ID3D10Blob* gsBlob = NULL;
	if (geoShader)
	{
		if (FAILED(D3DCompileFromFile(
			GSPath,
			NULL,
			includeHandler,
			"GSMain",
			shaderCompilerVersions[ShaderType::GS],
			flags,
			0,
			&gsBlob,
			&errorMessage)))
		{
			HandleCompileError(errorMessage, gspath);
		}
	}
	// pixel shader
	ID3D10Blob* psBlob = NULL;
	if (FAILED(D3DCompileFromFile(
		PSPath,
		NULL,
		includeHandler,
		"PSMain",
		shaderCompilerVersions[ShaderType::PS],
		flags,
		0,
		&psBlob,
		&errorMessage)))
	{
		HandleCompileError(errorMessage, pspath);
	}
	
	SetReflections(vsBlob, psBlob, gsBlob);
	//CheckSignatures();


	// CREATE SHADER PROGRAMS
	//---------------------------------------------------------------------------
	//create vertex shader buffer
	// TODO: specify which shaders to compile. some might not need pixel shader
	result = device->CreateVertexShader(vsBlob->GetBufferPointer(), 
										vsBlob->GetBufferSize(), 
										NULL, 
										&m_vertexShader);
	if (FAILED(result))
	{
		OutputDebugString("Error creating vertex shader program");
		assert(false);
	}

	//create pixel shader buffer
	result = device->CreatePixelShader(psBlob->GetBufferPointer(), 
										psBlob->GetBufferSize(), 
										NULL, 
										&m_pixelShader);
	if (FAILED(result))
	{
		OutputDebugString("Error creating pixel shader program");
		assert(false);
	}

	// create geo shader buffer
	if (geoShader)
	{
		result = device->CreateGeometryShader(  gsBlob->GetBufferPointer(),
												gsBlob->GetBufferSize(),
												NULL,
												&m_geometryShader);
		if (FAILED(result))
		{
			OutputDebugString("Error creating geometry shader program");
			assert(false);
		}
	}


	// INPUT LAYOUT
	//---------------------------------------------------------------------------
#if 1
	//setup the layout of the data that goes into the shader
	std::vector<D3D11_INPUT_ELEMENT_DESC> inputLayout(layouts.size());
	D3D11_SHADER_DESC shaderDesc = {};
	m_vsRefl->GetDesc(&shaderDesc);
	shaderDesc.InputParameters;
	D3D_PRIMITIVE primitiveDesc = shaderDesc.InputPrimitive;

	UINT sz = static_cast<UINT>(layouts.size());
	for (unsigned i = 0; i < layouts.size(); ++i)
	{
		inputLayout[i].SemanticName			= layouts[i].semanticName.c_str();
		inputLayout[i].SemanticIndex		= 0;	// TODO: process string for semantic index
		inputLayout[i].Format				= static_cast<DXGI_FORMAT>(layouts[i].format);
		inputLayout[i].InputSlot			= 0;
		//inputLayout[i].AlignedByteOffset	= i == layouts.size() - 1 ? 8 : 12;
		inputLayout[i].AlignedByteOffset	= i == 0 ? 0 : D3D11_APPEND_ALIGNED_ELEMENT;
		inputLayout[i].InputSlotClass		= D3D11_INPUT_PER_VERTEX_DATA;
		inputLayout[i].InstanceDataStepRate	= 0;
	}
	result = device->CreateInputLayout(	inputLayout.data(), 
										sz, 
										vsBlob->GetBufferPointer(),
										vsBlob->GetBufferSize(), 
										&m_layout);
	if (FAILED(result))
	{
		OutputDebugString("Error creating input layout");
		assert(false);
	}
#else
	// todo: get layout from reflection and handle cpu input layouts
	// https://takinginitiative.wordpress.com/2011/12/11/directx-1011-basic-shader-reflection-automatic-input-layout-creation/
#endif

	// CBUFFERS & SHADER RESOURCES
	//---------------------------------------------------------------------------
	SetConstantBuffers(device);
	
	// SET TEXTURES & SAMPLERS
	auto sRefl = m_psRefl;		// vsRefl? gsRefl?
	D3D11_SHADER_DESC desc;
	sRefl->GetDesc(&desc);

	unsigned texSlot = 0;
	unsigned smpSlot = 0;
	for (unsigned i = 0; i < desc.BoundResources; ++i)
	{
		D3D11_SHADER_INPUT_BIND_DESC shdInpDesc;
		sRefl->GetResourceBindingDesc(i, &shdInpDesc);
		if (shdInpDesc.Type == D3D_SIT_SAMPLER)
		{
			ShaderSampler smp;
			smp.name = shdInpDesc.Name;
			smp.shdType = ShaderType::PS;
			smp.bufferSlot = smpSlot++;
			m_samplers.push_back(smp);
		}
		else if (shdInpDesc.Type == D3D_SIT_TEXTURE)
		{
			ShaderTexture tex;
			tex.name = shdInpDesc.Name;
			tex.shdType = ShaderType::PS;
			tex.bufferSlot = texSlot++;
			m_textures.push_back(tex);
		}
	}

	//release shader buffers
	vsBlob->Release();
	vsBlob = 0;

	psBlob->Release();
	psBlob = 0;

	if (gsBlob)
	{
		gsBlob->Release();
		gsBlob = 0;
	}
	OutputDebugString(" - Done.\n");
}

void Shader::SetReflections(ID3D10Blob* vsBlob, ID3D10Blob* psBlob, ID3D10Blob* gsBlob)
{
	// Vertex Shader
	if (FAILED(D3DReflect(
		vsBlob->GetBufferPointer(),
		vsBlob->GetBufferSize(),
		IID_ID3D11ShaderReflection,
		(void**)&m_vsRefl)))
	{
		OutputDebugString("Error getting vertex shader reflection\n");
		assert(false);
	}

	// Pixel Shader
	if (FAILED(D3DReflect(
		psBlob->GetBufferPointer(),
		psBlob->GetBufferSize(),
		IID_ID3D11ShaderReflection,
		(void**)&m_psRefl)))
	{
		OutputDebugString("Error getting pixel shader reflection\n");
		assert(false);
	}

	// Geometry Shader
	if (gsBlob)
	{
		if (FAILED(D3DReflect(
			gsBlob->GetBufferPointer(),
			gsBlob->GetBufferSize(),
			IID_ID3D11ShaderReflection,
			(void**)&m_gsRefl)))
		{
			OutputDebugString("Error getting pixel shader reflection\n");
			assert(false);
		}
	}
}

void Shader::CheckSignatures()
{
	// get shader description --> input/output parameters
	std::vector<D3D11_SIGNATURE_PARAMETER_DESC> VSISignDescs, VSOSignDescs, PSISignDescs, PSOSignDescs;
	D3D11_SHADER_DESC VSDesc;
	m_vsRefl->GetDesc(&VSDesc);
	for (unsigned i = 0; i < VSDesc.InputParameters; ++i)
	{
		D3D11_SIGNATURE_PARAMETER_DESC input_desc;
		m_vsRefl->GetInputParameterDesc(i, &input_desc);
		VSISignDescs.push_back(input_desc);
	}

	for (unsigned i = 0; i < VSDesc.OutputParameters; ++i)
	{
		D3D11_SIGNATURE_PARAMETER_DESC output_desc;
		m_vsRefl->GetInputParameterDesc(i, &output_desc);
		VSOSignDescs.push_back(output_desc);
	}


	D3D11_SHADER_DESC PSDesc;
	m_psRefl->GetDesc(&PSDesc);
	for (unsigned i = 0; i < PSDesc.InputParameters; ++i)
	{
		D3D11_SIGNATURE_PARAMETER_DESC input_desc;
		m_psRefl->GetInputParameterDesc(i, &input_desc);
		PSISignDescs.push_back(input_desc);
	}

	for (unsigned i = 0; i < PSDesc.OutputParameters; ++i)
	{
		D3D11_SIGNATURE_PARAMETER_DESC output_desc;
		m_psRefl->GetInputParameterDesc(i, &output_desc);
		PSOSignDescs.push_back(output_desc);
	}

	// check VS-PS signature compatibility | wont be necessary when its 1 file.
	// THIS IS TEMPORARY
	if (VSOSignDescs.size() != PSISignDescs.size())
	{
		OutputDebugString("Error: Incompatible shader input/output signatures (sizes don't match)\n");
		assert(false);
	}
	else
	{
		for (size_t i = 0; i < VSOSignDescs.size(); ++i)
		{
			// TODO: order matters, semantic slot doesnt. check order
			;
		}
	}
}

void Shader::SetConstantBuffers(ID3D11Device* device)
{
	// example: http://gamedev.stackexchange.com/a/62395/39920

	// OBTAIN CBUFFER LAYOUT INFORMATION
	//---------------------------------------------------------------------------------------
	RegisterConstantBufferLayout(m_vsRefl, ShaderType::VS);
	RegisterConstantBufferLayout(m_psRefl, ShaderType::PS);
	if(m_gsRefl) RegisterConstantBufferLayout(m_gsRefl, ShaderType::GS);

	// CREATE CPU & GPU CONSTANT BUFFERS
	//---------------------------------------------------------------------------------------
	// CPU CBuffers
	int constantBufferSlot = 0;
	for (const ConstantBufferLayout& cbLayout : m_CBLayouts)
	{
		std::vector<CPUConstantID> cpuBuffers;
		for (D3D11_SHADER_VARIABLE_DESC varDesc : cbLayout.variables)
		{
			auto& next = CPUConstant::GetNextAvailable();
			CPUConstantID c_id = std::get<1>(next);
			CPUConstant& c = std::get<0>(next);

			c._name = varDesc.Name;
			c._size = varDesc.Size;
			c._data = new char[c._size];
			memset(c._data, 0, c._size);
			m_constants.push_back(std::make_pair(constantBufferSlot, c_id));
			
		}
		++constantBufferSlot;
	}

	//LogConstantBufferLayouts();
	m_constantsUnsorted = m_constants;
	std::sort(m_constants.begin(), m_constants.end(), [](const ConstantBufferMapping& lhs, const ConstantBufferMapping& rhs) {
		const std::string& lstr = CPUConstant::Get(lhs.second)._name;	const std::string& rstr = CPUConstant::Get(rhs.second)._name;
		return lstr <= rstr;
	});
	//LogConstantBufferLayouts();

	// GPU CBuffer Description
	D3D11_BUFFER_DESC cBufferDesc;
	cBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	cBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cBufferDesc.MiscFlags = 0;
	cBufferDesc.StructureByteStride = 0;

	// GPU CBuffers
	for (const ConstantBufferLayout& cbLayout : m_CBLayouts)
	{
		ConstantBuffer cBuffer;
		cBufferDesc.ByteWidth = cbLayout.desc.Size;
		if (FAILED(device->CreateBuffer(&cBufferDesc, NULL, &cBuffer.data)))
		{
			OutputDebugString("Error creating constant buffer");
			assert(false);
		}
		cBuffer.dirty = true;
		cBuffer.shdType = cbLayout.shdType;
		cBuffer.bufferSlot = cbLayout.bufSlot;
		m_cBuffers.push_back(cBuffer);
	}
}

void Shader::LogConstantBufferLayouts() const
{
	char inputTable[2048];
	sprintf_s(inputTable, "\n%s ConstantBuffers: -----\n", this->m_name.c_str());
	std::for_each(m_constants.begin(), m_constants.end(), [&inputTable](const ConstantBufferMapping& cMapping) {
		char entry[32];
		sprintf_s(entry, "(%d, %d)\t- %s\n", cMapping.first, cMapping.second, CPUConstant::Get(cMapping.second)._name.c_str());
		strcat_s(inputTable, entry);
	});
	strcat_s(inputTable, "-----\n");
	Log::Info(std::string(inputTable));
}

void Shader::RegisterConstantBufferLayout(ID3D11ShaderReflection* sRefl, ShaderType type)
{
	D3D11_SHADER_DESC desc;
	sRefl->GetDesc(&desc);

	unsigned bufSlot = 0;
	for (unsigned i = 0; i < desc.ConstantBuffers; ++i)
	{
		ConstantBufferLayout bufferLayout;
		bufferLayout.buffSize = 0;
		ID3D11ShaderReflectionConstantBuffer* pCBuffer = sRefl->GetConstantBufferByIndex(i);
		pCBuffer->GetDesc(&bufferLayout.desc);

		// load desc of each variable for binding on buffer later on
		for (unsigned j = 0; j < bufferLayout.desc.Variables; ++j)
		{
			// get variable and type descriptions
			ID3D11ShaderReflectionVariable* pVariable = pCBuffer->GetVariableByIndex(j);
			D3D11_SHADER_VARIABLE_DESC varDesc;
			pVariable->GetDesc(&varDesc);
			bufferLayout.variables.push_back(varDesc);

			ID3D11ShaderReflectionType* pType = pVariable->GetType();
			D3D11_SHADER_TYPE_DESC typeDesc;
			pType->GetDesc(&typeDesc);
			bufferLayout.types.push_back(typeDesc);

			// accumulate buffer size
			bufferLayout.buffSize += varDesc.Size;
		}
		bufferLayout.shdType = type;
		bufferLayout.bufSlot = bufSlot;
		++bufSlot;
		m_CBLayouts.push_back(bufferLayout);
	}
}

void Shader::ClearConstantBuffers()
{
	for (ConstantBuffer& cBuffer : m_cBuffers)
	{
		cBuffer.dirty = true;
	}
}

void Shader::UpdateConstants(ID3D11DeviceContext* context)
{
	for (unsigned i = 0; i < m_cBuffers.size(); ++i)
	{
		ConstantBuffer& CB = m_cBuffers[i];
		if (CB.dirty)	// if the CPU-side buffer is updated
		{
			ID3D11Buffer* data = CB.data;
			D3D11_MAPPED_SUBRESOURCE mappedResource;

			// Map sub-resource to GPU - update contents - discard the sub-resource
			context->Map(data, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			char* bufferPos = static_cast<char*>(mappedResource.pData);	// char* so we can advance the pointer
			for (const ConstantBufferMapping& indexIDPair : m_constantsUnsorted)
			{
				if (indexIDPair.first != i) continue;
				const int slotIndex = indexIDPair.first;
				const CPUConstantID c_id = indexIDPair.second;
				CPUConstant& c = CPUConstant::Get(c_id);
				memcpy(bufferPos, c._data, c._size);
				bufferPos += c._size;
			}
			context->Unmap(data, 0);

			// TODO: research update sub-resource (Setting constant buffer can be done once in setting the shader, see it)

			// call XSSetConstantBuffers() from array using ShaderType enum
			(context->*SetShaderConstants[CB.shdType])(CB.bufferSlot, 1, &data);
			CB.dirty = false;
		}
	}
}

const std::string& Shader::Name() const
{
	return m_name;
}

ShaderID Shader::ID() const
{
	return m_id;
}

const std::vector<ConstantBufferLayout>& Shader::GetConstantBufferLayouts() const
{
	return m_CBLayouts;
}

const std::vector<ConstantBuffer>& Shader::GetConstantBuffers() const
{
	return m_cBuffers;
}

std::tuple<CPUConstant&, CPUConstantID> CPUConstant::GetNextAvailable()
{
	const CPUConstantID id = static_cast<CPUConstantID>(s_nextConstIndex++);
	return std::make_tuple(std::ref(s_constants[id]), id);
}

void CPUConstant::CleanUp()
{
	for (size_t i = 0; i < MAX_CONSTANT_BUFFERS; i++)
	{
		if (s_constants[i]._data)
		{
			delete s_constants[i]._data;
			s_constants[i]._data = nullptr;
		}
	}
}