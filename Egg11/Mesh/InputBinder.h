#pragma once
#include <vector>
#include "Geometry.h"

namespace Egg11 { namespace Mesh {

	GG_CLASS(InputBinder)
		class InputConfiguration
		{
			friend class Egg11::Mesh::InputBinder;

			const D3D11_INPUT_ELEMENT_DESC* elements;
			unsigned int nElements;
			Microsoft::WRL::ComPtr<ID3DBlob> byteCode;

			Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;

			InputConfiguration(Microsoft::WRL::ComPtr<ID3DBlob> byteCode, Geometry::P geometry);

			/// Returns true if input signatures are identical and elements with shared semantics are also identical.
			bool isCompatible(const InputConfiguration& other) const;

			HRESULT createInputLayout(Microsoft::WRL::ComPtr<ID3D11Device> device);
		public:
			~InputConfiguration();
		};

		using InputConfigurationList = std::vector<InputConfiguration*>;
		InputConfigurationList inputConfigurationList;

		Microsoft::WRL::ComPtr<ID3D11Device> device;
	protected:
		InputBinder(Microsoft::WRL::ComPtr<ID3D11Device> device);
	public:

		~InputBinder();

		Microsoft::WRL::ComPtr<ID3D11InputLayout> getCompatibleInputLayout(Microsoft::WRL::ComPtr<ID3DBlob> byteCode, Geometry::P geometry);

	GG_ENDCLASS

}} // namespace Egg11::Mesh