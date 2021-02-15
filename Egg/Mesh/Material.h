#pragma once

#include "../Common.h"
#include <map>

namespace Egg {
	namespace Mesh {

		GG_CLASS(Material)

			com_ptr<ID3D12RootSignature> rootSignature;
			com_ptr<ID3DBlob> vertexShader;
			com_ptr<ID3DBlob> hullShader;
			com_ptr<ID3DBlob> domainShader;
			com_ptr<ID3DBlob> geometryShader;
			com_ptr<ID3DBlob> pixelShader;
			D3D12_BLEND_DESC blendState;
			D3D12_RASTERIZER_DESC rasterizerState;
			D3D12_DEPTH_STENCIL_DESC depthStencilState;
			DXGI_FORMAT dsvFormat;

			com_ptr<ID3D12RootSignatureDeserializer> rsDeserializer;
			com_ptr<ID3D12ShaderReflection> vsReflection;
			com_ptr<ID3D12ShaderReflection> hsReflection;
			com_ptr<ID3D12ShaderReflection> dsReflection;
			com_ptr<ID3D12ShaderReflection> gsReflection;
			com_ptr<ID3D12ShaderReflection> psReflection;

			struct BufferAddressing {
				D3D12_GPU_VIRTUAL_ADDRESS address;
				unsigned int perObjectByteStride;
			};
			std::map<unsigned int, BufferAddressing >
				constantBufferBindings;
			com_ptr<ID3D12DescriptorHeap> srvHeap;
			unsigned int srvHeapByteOffset;
			unsigned int srvDescriptorTableRootParameterIndex;

		public:

			Material() : rootSignature{ nullptr }, vertexShader{ nullptr }, geometryShader{ nullptr }, pixelShader{ nullptr }, blendState{}, rasterizerState{}, depthStencilState{}
				, rsDeserializer{ nullptr }, vsReflection{ nullptr }, gsReflection{ nullptr }, psReflection{ nullptr },
				srvHeap{nullptr},
				domainShader{ nullptr }, hullShader{nullptr},
				dsReflection{ nullptr }, hsReflection{nullptr}
			{
				blendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				rasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				depthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
				depthStencilState.DepthEnable = FALSE;
				depthStencilState.StencilEnable = FALSE;
				dsvFormat = DXGI_FORMAT_UNKNOWN;
			}

			void SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC & dsd) {
				depthStencilState = dsd;
			}

			void SetRasterizerState(const D3D12_RASTERIZER_DESC & rsd) {
				rasterizerState = rsd;
			}

			void SetBlendState(const D3D12_BLEND_DESC & bsd) {
				blendState = bsd;
			}

			void SetRootSignature(com_ptr<ID3D12RootSignature> rs) {
				rootSignature = rs;
			}

			void SetVertexShader(com_ptr<ID3DBlob> vs) {
				vertexShader = vs;
				DX_API("Failed to reflect vertex shader")
					D3DReflect(vs->GetBufferPointer(), vs->GetBufferSize(), IID_PPV_ARGS(vsReflection.GetAddressOf()));

				DX_API("Failed to deserialize root signature")
					D3D12CreateRootSignatureDeserializer(vs->GetBufferPointer(), vs->GetBufferSize(), IID_PPV_ARGS(rsDeserializer.GetAddressOf()));

			}

			void SetPixelShader(com_ptr<ID3DBlob> ps) {
				pixelShader = ps;
				DX_API("Failed to reflect pixel shader")
					D3DReflect(ps->GetBufferPointer(), ps->GetBufferSize(), IID_PPV_ARGS(psReflection.GetAddressOf()));
			}

			void SetGeometryShader(com_ptr<ID3DBlob> gs) {
				geometryShader = gs;
				if (gs != nullptr) {
					DX_API("Failed to reflect geometry shader")
						D3DReflect(gs->GetBufferPointer(), gs->GetBufferSize(), IID_PPV_ARGS(gsReflection.GetAddressOf()));
				}
			}

			void SetHullShader(com_ptr<ID3DBlob> hs) {
				hullShader = hs;
				DX_API("Failed to reflect pixel shader")
					D3DReflect(hs->GetBufferPointer(), hs->GetBufferSize(), IID_PPV_ARGS(hsReflection.GetAddressOf()));
			}

			void SetDomainShader(com_ptr<ID3DBlob> ds) {
				domainShader = ds;
				DX_API("Failed to reflect pixel shader")
					D3DReflect(ds->GetBufferPointer(), ds->GetBufferSize(), IID_PPV_ARGS(dsReflection.GetAddressOf()));
			}


			void SetSrvHeap(
					unsigned int srvDescriptorTableRootParameterIndex,
					com_ptr<ID3D12DescriptorHeap> srvHeap,
					unsigned int materialByteOffset = 0
				) {
				this->srvDescriptorTableRootParameterIndex = srvDescriptorTableRootParameterIndex;
				this->srvHeap = srvHeap;
				this->srvHeapByteOffset = materialByteOffset;
			}

			void SetDSVFormat(DXGI_FORMAT format) {
				dsvFormat = format;
			}

			void ApplyToDescriptor(D3D12_GRAPHICS_PIPELINE_STATE_DESC & psoDesc) {
				psoDesc.pRootSignature = rootSignature.Get();
				psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
				if(geometryShader != nullptr) {
					psoDesc.GS = CD3DX12_SHADER_BYTECODE(geometryShader.Get());
				}
				if (hullShader != nullptr) {
					psoDesc.HS = CD3DX12_SHADER_BYTECODE(hullShader.Get());
				}
				if (domainShader != nullptr) {
					psoDesc.DS = CD3DX12_SHADER_BYTECODE(domainShader.Get());
				}
				psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
				psoDesc.BlendState = blendState;
				psoDesc.RasterizerState = rasterizerState;
				psoDesc.DepthStencilState = depthStencilState;
				psoDesc.DSVFormat = dsvFormat;
				psoDesc.SampleDesc.Count = 1;
				psoDesc.SampleDesc.Quality = 0;
				psoDesc.SampleMask = UINT_MAX;
			}

			void ApplyToCommandList(ID3D12GraphicsCommandList* commandList, unsigned int objectIndex = 0) {
				commandList->SetGraphicsRootSignature(rootSignature.Get());
				for (auto& binding : constantBufferBindings) {
					commandList->SetGraphicsRootConstantBufferView(
						binding.first,
						binding.second.address +
						binding.second.perObjectByteStride * objectIndex
					);
				}
				if (srvHeap) {
					ID3D12DescriptorHeap* descriptorHeaps[] = { srvHeap.Get() };
					commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
					CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle{
						srvHeap->GetGPUDescriptorHandleForHeapStart(),
						(int)srvHeapByteOffset };
					commandList->SetGraphicsRootDescriptorTable(srvDescriptorTableRootParameterIndex,
						gpuHandle);
				}
			}

			template<typename T>
			void SetConstantBuffer(T& resource, unsigned int perObjectByteStride = 0) {
				SetConstantBuffer(resource, perObjectByteStride, typeid(T::Type).name());
			}

			template<typename T>
			void SetConstantBuffer(T& resource, unsigned int perObjectByteStride, const std::string& nameOverride) {
				std::string name = nameOverride;

				size_t indexOf;
				if ((indexOf = name.find(' ')) != std::string::npos) {
					name = name.substr(indexOf + 1);
				}

				D3D12_SHADER_INPUT_BIND_DESC bindDesc;

				ID3D12ShaderReflection* reflections[] = {
					vsReflection.Get(), gsReflection.Get(), psReflection.Get(),
					hsReflection.Get(), dsReflection.Get()
				};

				HRESULT hr;
				for (unsigned int i = 0; i < _countof(reflections); ++i) {
					if (!reflections[i]) {
						continue;
					}

					hr = reflections[i]->GetResourceBindingDescByName(name.c_str(), &bindDesc);

					if (SUCCEEDED(hr)) {
						break;
					}
				}

				ASSERT(SUCCEEDED(hr), "Failed to find constant buffer '%s'\r\nPossible errors:\r\n-Optimized away\r\n-Name mismatch\r\n-Wrong shader used", name.c_str());

				const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc = *(rsDeserializer->GetRootSignatureDesc());
				for (unsigned int i = 0; i < rootSignatureDesc.NumParameters; ++i) {
					const D3D12_ROOT_PARAMETER& param = rootSignatureDesc.pParameters[i];
					if (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV &&
						param.Descriptor.ShaderRegister == bindDesc.BindPoint &&
						param.Descriptor.RegisterSpace == bindDesc.Space) {
						constantBufferBindings[i] = {
							resource.GetGPUVirtualAddress(),
							perObjectByteStride
						};
						//commandList->SetGraphicsRootConstantBufferView(i, resource.GetGPUVirtualAddress());
						return;
					}
				}

				ASSERT(false, "Failed to bind constant buffer '%s' to root signature.\r\nPossible errors:\r\n-Wrong root signature used on Vertex Shader", name.c_str());
			}

		GG_ENDCLASS

	}
}
