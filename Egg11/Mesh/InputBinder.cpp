#include "stdafx.h"
#include "InputBinder.h"

using namespace Egg11::Mesh;

InputBinder::InputBinder(Microsoft::WRL::ComPtr<ID3D11Device> device)
	:device(device)
{
}

InputBinder::~InputBinder()
{
	InputConfigurationList::iterator i = inputConfigurationList.begin();
	InputConfigurationList::iterator e = inputConfigurationList.end();
	while(i != e)
	{
		delete *i;
		i++;
	}
}

InputBinder::InputConfiguration::InputConfiguration(Microsoft::WRL::ComPtr<ID3DBlob> byteCode, Geometry::P geometry)
{
	this->byteCode = byteCode;

	const D3D11_INPUT_ELEMENT_DESC* delements;
	unsigned int dnElements;
	geometry->getElements(delements, dnElements);

	nElements = dnElements;

	D3D11_INPUT_ELEMENT_DESC* elements = new D3D11_INPUT_ELEMENT_DESC[nElements];
	memcpy(elements, delements, nElements * sizeof(D3D11_INPUT_ELEMENT_DESC));

	for(uint i=0; i<nElements; i++)
	{
		unsigned int nChar = strlen(delements[i].SemanticName) + 1;
		char* semanticName = new char[nChar];
		strcpy_s(semanticName, nChar, delements[i].SemanticName);
		elements[i].SemanticName = semanticName;
	}
	this->elements = elements;
}

InputBinder::InputConfiguration::~InputConfiguration()
{
	for(uint i=0; i<nElements; i++)
		if(elements[i].SemanticName)
			delete elements[i].SemanticName;
	delete [] elements;
}

HRESULT InputBinder::InputConfiguration::createInputLayout(Microsoft::WRL::ComPtr<ID3D11Device> device)
{
	return device->CreateInputLayout(elements, nElements, byteCode->GetBufferPointer(), byteCode->GetBufferSize(), &inputLayout);
}

bool InputBinder::InputConfiguration::isCompatible(const InputBinder::InputConfiguration& other) const
{

	if(byteCode->GetBufferSize() != other.byteCode->GetBufferSize())
		return false;
	if(0 != memcmp(byteCode->GetBufferPointer(), other.byteCode->GetBufferPointer(), byteCode->GetBufferSize()))
		return false;
	for(unsigned int i=0; i<nElements; i++)
	{
		for(unsigned int u=0; u<other.nElements; u++)
		{
			bool found = false;
			if(elements[i].SemanticIndex == other.elements[u].SemanticIndex &&
				(0 == strcmp(elements[i].SemanticName, other.elements[u].SemanticName)))
			{
				found = true;
				if(elements[i].AlignedByteOffset != other.elements[u].AlignedByteOffset)
					return false;
				if(elements[i].Format != other.elements[u].Format)
					return false;
				if(elements[i].InputSlot != other.elements[u].InputSlot)
					return false;
				if(elements[i].InputSlotClass != other.elements[u].InputSlotClass)
					return false;
				if(elements[i].InstanceDataStepRate != other.elements[u].InstanceDataStepRate)
					return false;
			}
			if(!found)
				return false;
		}
	}
	return true;
}

Microsoft::WRL::ComPtr<ID3D11InputLayout> InputBinder::getCompatibleInputLayout(Microsoft::WRL::ComPtr<ID3DBlob> byteCode, Geometry::P geometry)
{
	if(byteCode == nullptr)
		return nullptr; // TODO warn, this is legal for a compute shader, which does not require an input layout at all
	InputConfiguration* tentativeInputConfiguration = new InputConfiguration(byteCode, geometry);
	InputConfigurationList::iterator i = inputConfigurationList.begin();
	InputConfigurationList::iterator e = inputConfigurationList.end();
#ifdef DEBUG
	if(tentativeInputConfiguration->createInputLayout(device) != S_OK)
	{
		// TODO mesh throw error handling
		MessageBox(NULL, L"Failed to create an input layout. Invalid mesh-technique combination.", L"Error!", 0);
		DXUTShutdown(-1);
	}
	while(i != e)
	{
		if((*i)->isCompatible(*tentativeInputConfiguration))
		{
			tentativeInputConfiguration->inputLayout->Release();
			delete tentativeInputConfiguration;
			return (*i)->inputLayout;
		}
		i++;
	}
#else
	while(i != e)
	{
		if((*i)->isCompatible(*tentativeInputConfiguration))
			return (*i)->inputLayout;
		i++;
	}
	tentativeInputConfiguration->createInputLayout(device);
#endif
	inputConfigurationList.push_back(tentativeInputConfiguration);
	return tentativeInputConfiguration->inputLayout;
}

