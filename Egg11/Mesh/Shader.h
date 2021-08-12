#pragma once

#include <string>
#include <D3D11Shader.h>

namespace Egg11 { namespace Mesh {

	GG_CLASS(Shader)
		std::string filename;
		Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflection;
		Microsoft::WRL::ComPtr<ID3D11DeviceChild> shader;
		Microsoft::WRL::ComPtr<ID3DBlob> byteCode;

	protected:
		Shader(
			std::string filename,
			Microsoft::WRL::ComPtr<ID3D11Device> device,
			Microsoft::WRL::ComPtr<ID3DBlob> byteCode);
	public:
		Microsoft::WRL::ComPtr<ID3D11DeviceChild> getShader()
		{
			return shader;
		}


		Microsoft::WRL::ComPtr<ID3DBlob> getByteCode()
		{
			return byteCode;
		}

		template<class ID3D11ShaderType>
		Microsoft::WRL::ComPtr<ID3D11ShaderType> as()
		{
			Microsoft::WRL::ComPtr<ID3D11ShaderType> vs;
			if (shader.As(&vs) == S_OK)
				return vs;
			else
				return nullptr;
		}

		Microsoft::WRL::ComPtr<ID3D11ShaderReflection> getReflection()
		{
			return reflection;
		}

		std::string getFilename()
		{
			return filename;
		}

		uint getResourceIndex(std::string name);
	GG_ENDCLASS

}} // namespace Egg11::Mesh
