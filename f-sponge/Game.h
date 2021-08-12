#pragma once
#include "Egg11/App/App.h"
#include "Egg11/Mesh/InputBinder.h"
#include "Egg11/Mesh/Shaded.h"
#include "Egg11/Mesh/VertexStream.h"
#include "Egg11/Mesh/Nothing.h"
#include "Egg11/Cam/FirstPerson.h"
#include "Particle.h"

#include <array>

class DualQuaternion;

GG_SUBCLASS(Game, Egg11::App)
public:	
	//static const unsigned int windowHeight = 720;
	//static const unsigned int windowWidth = 1280;

	static const unsigned int windowHeight = 1024;
	static const unsigned int windowWidth = 1024;

	static const unsigned int fillWindowHeight = 200;
	static const unsigned int fillWindowWidth = 200;

	static const unsigned int counterSize = 3;

private:
	enum BillboardsAlgorithm { Normal, ABuffer, SBuffer, SBufferV2, HashSimple };
	enum RenderMode { Realistic, Gradient, ControlParticles, Particles };
	enum FlowControl { RealisticFlow, ControlledFlow };
	enum ControlParticlePlacement { Vertex, Render, Animated, PBD };	
	enum Metal { Aluminium, Copper, Gold };
	enum Shading { MetalShading, PhongShading };
	enum MetaballFunction {Simple, Wyvill, Nishimura, Murakami};
	enum WaterShading { SimpleWater, DeepWater };

	// Common
	Egg11::Cam::FirstPerson::P firstPersonCam;
	Microsoft::WRL::ComPtr<ID3D11Buffer> modelViewProjCB;
	Microsoft::WRL::ComPtr<ID3D11Buffer> eyePosCB;
	Microsoft::WRL::ComPtr<ID3D11Buffer> shadingCB;
	Microsoft::WRL::ComPtr<ID3D11Buffer> shadingTypeCB;
	Microsoft::WRL::ComPtr<ID3D11Buffer> metaballFunctionCB;

	Egg11::Mesh::InputBinder::P inputBinder;

	BillboardsAlgorithm billboardsLoadAlgorithm;
	RenderMode renderMode;
	FlowControl flowControl;
	ControlParticlePlacement controlParticlePlacement;
	Metal metalShading;
	Shading shading;
	MetaballFunction metaballFunction;
	WaterShading waterShading;
	bool drawFlatControlMesh;
	bool animtedIsActive;
	bool adapticeControlPressureIsActive;
	bool controlParticleAnimtaionIsActive;

	// Particle
	Microsoft::WRL::ComPtr<ID3D11Buffer> particleDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particleUAV;

	// Control Particle
	Microsoft::WRL::ComPtr<ID3D11Buffer> controlParticleDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> controlParticleSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> controlParticleUAV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> controlParticleCounterDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> controlParticleCounterSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> controlParticleCounterUAV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> controlParticleIndirectDisptachDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> controlParticleIndirectDisptachUAV;
	Egg11::Mesh::Shaded::P controlMesh;
	Egg11::Mesh::Shaded::P controlMeshFlat; //Debug
	Egg11::Mesh::Shaded::P controlMeshFill;
	float controlMeshScale;
	Egg11::Cam::FirstPerson::P fillCam;
	Egg11::Mesh::Shaded::P animatedControlMesh;
	Egg11::Mesh::Shaded::P animatedControlMeshFlat;
	float animatedControlMeshScale;

	// PBD
	Microsoft::WRL::ComPtr<ID3D11Buffer> controlParticleNewPosDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> controlParticleNewPosSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> controlParticleNewPosUAV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> controlParticleVelocityDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> controlParticleVelocitySRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> controlParticleVelocityUAV;
	Egg11::Mesh::Shader::P PBDShaderGravity;
	Egg11::Mesh::Shader::P PBDShaderCollision;
	Egg11::Mesh::Shader::P PBDShaderDistance;
	Egg11::Mesh::Shader::P PBDShaderBending;
	Egg11::Mesh::Shader::P PBDShaderFinalUpdate;

	// Hashtables
	Microsoft::WRL::ComPtr<ID3D11Buffer> clistDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> clistSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> clistUAV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> clistLengthDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> clistLengthSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> clistLengthUAV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> clistBeginDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> clistBeginSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> clistBeginUAV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> clistCellCountDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> clistCellCountSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> clistCellCountUAV;

	Microsoft::WRL::ComPtr<ID3D11Buffer> hlistDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> hlistSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> hlistUAV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> hlistLengthDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> hlistLengthSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> hlistLengthUAV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> hlistBeginDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> hlistBeginSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> hlistBeginUAV;

	Egg11::Mesh::Shader::P clistShaderInit;
	Egg11::Mesh::Shader::P clistShaderCompact;
	Egg11::Mesh::Shader::P clistShaderLength;
	Egg11::Mesh::Shader::P clistShaderSortEven;
	Egg11::Mesh::Shader::P clistShaderSortOdd;
	Egg11::Mesh::Shader::P hlistShaderInit;
	Egg11::Mesh::Shader::P hlistShaderBegin;
	Egg11::Mesh::Shader::P hlistShaderLength;

	// Billboard
	Egg11::Mesh::Nothing::P billboardNothing;	
	Microsoft::WRL::ComPtr<ID3D11Buffer> billboardSizeCB;

	Egg11::Mesh::Shaded::P billboards;
	Egg11::Mesh::Shaded::P billboardsControl;
	Egg11::Mesh::Shader::P billboardsPixelShaderA;
	Egg11::Mesh::Shader::P billboardsPixelShaderS1;
	Egg11::Mesh::Shader::P billboardsPixelShaderS2;
	Egg11::Mesh::Shader::P billboardsPixelShaderSV21;
	Egg11::Mesh::Shader::P billboardsPixelShaderSV22;

	Microsoft::WRL::ComPtr<ID3D11Buffer> offsetBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> offsetSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> offsetUAV;

	Microsoft::WRL::ComPtr<ID3D11Buffer> linkBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> linkSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> linkUAV;

	Microsoft::WRL::ComPtr<ID3D11Buffer> idBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> idSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> idUAV;

	Microsoft::WRL::ComPtr<ID3D11Buffer> countBuffer;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> countUAV;

	Microsoft::WRL::ComPtr<ID3D11Buffer> counterBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> counterSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> counterUAV;


	// Prefix sum
	Microsoft::WRL::ComPtr<ID3D11Buffer> scanBucketSizeCB;

	Microsoft::WRL::ComPtr<ID3D11Buffer> resultBuffer;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> resultUAV;

	Egg11::Mesh::Shader::P prefixSumComputeShader;
	Egg11::Mesh::Shader::P prefixSumScanBucketResultShader;
	Egg11::Mesh::Shader::P prefixSumScanAddBucketResultShader;

	Egg11::Mesh::Shader::P prefixSumV2ComputeShader;


	// Environment
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> envSrv;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;


	// Metaball
	int testCount;
	int testCount2;
	float radius;
	float metaBallMinToHit;
	int binaryStepCount;
	int maxRecursion;
	int marchCount;
	Egg11::Mesh::Shaded::P metaballs;
	Egg11::Mesh::Shader::P metaballRealisticPixelShader;
	Egg11::Mesh::Shader::P metaballGradientPixelShader;
	Egg11::Mesh::Shader::P metaballRealisticAPixelShader;
	Egg11::Mesh::Shader::P metaballGradientAPixelShader;
	Egg11::Mesh::Shader::P metaballRealisticSPixelShader;
	Egg11::Mesh::Shader::P metaballGradientSPixelShader;
	Egg11::Mesh::Shader::P metaballRealisticS2PixelShader;
	Egg11::Mesh::Shader::P metaballGradientS2PixelShader;
	Egg11::Mesh::Shader::P metaballRealisticHashSimpleShader;
	Egg11::Mesh::Shader::P metaballGradientHashSimpleShader;

	// Animation
	Egg11::Mesh::Shader::P fluidSimulationShader;
	Egg11::Mesh::Shader::P controlledFluidSimulationShader;
	Egg11::Mesh::Shader::P simpleSortEvenShader;
	Egg11::Mesh::Shader::P simpleSortOddShader;
	Egg11::Mesh::Shader::P mortonHashShader;
	Egg11::Mesh::Shader::P setIndirectDispatchBufferShader;
	Egg11::Mesh::Shader::P adaptiveControlPressureShader;
	Egg11::Mesh::Shader::P rigControlParticlesShader;
	Egg11::Mesh::Shader::P animateControlParticlesShader;
	Microsoft::WRL::ComPtr<ID3D11Buffer> controlParamsCB;
	std::array<float, 8> controlParams;


	// Debug
	Egg11::Mesh::Nothing::P cpBillboardNothing;
	Microsoft::WRL::ComPtr<ID3D11Buffer> cpBillboardSizeCB;
	Egg11::Mesh::Shaded::P cpBillboards;
	Egg11::Mesh::Shader::P billboardsPixelShader;
	Egg11::Mesh::Shader::P controlParticleBallPixelShader;
	Egg11::Mesh::Shader::P particleBallPixelShader;
	Microsoft::WRL::ComPtr<ID3D11Buffer> debugTypeCB;
	uint debugType;
	const uint maxDebugType = 7;

	Microsoft::WRL::ComPtr<ID3D11Buffer> uavCounterReadback;

	// Skeletal
	int meshIdxInFile;
	float animatedMeshScale;
	int nBones;
	int nInstances;
	int nKeys;
	int nNodes;

	std::vector<std::string> boneNames;
	//	std::vector<Egg11::Math::float4x4> riggingPoseBoneTransforms;
	std::vector<std::vector<unsigned char> > boneTransformationChainNodeIndices;
	std::map<std::string, unsigned char> nodeNamesToNodeIndices;

	unsigned char* skeleton;
	DualQuaternion* keys;
	DualQuaternion* rigging;
	unsigned int currentKey;

	Microsoft::WRL::ComPtr<ID3D11Buffer> boneBuffer;

	Microsoft::WRL::ComPtr<ID3D11Buffer> bonePositionsBufferCB;

public:
	Game(Microsoft::WRL::ComPtr<ID3D11Device> device);
	~Game(void);

	HRESULT createResources();
	HRESULT releaseResources();

	void render(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void animate(double dt, double t);
	bool processMessage(HWND hWnd,
		UINT uMsg, WPARAM wParam, LPARAM lParam);


	void CreateCommon();
	void CreateParticles();
	void CreateControlMesh();
	void CreateControlParticles();
	void CreateBillboard();
	void CreateBillboardForControlParticles();
	void CreatePrefixSum();
	void CreateEnviroment();
	void CreateMetaball();
	void CreateAnimation();	
	void CreateEnvironment();
	void CreateDebug();

	void clearRenderTarget(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void clearContext(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

	void renderParticleBillboard(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderControlParticleBillboard(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderBillboardA(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderBillboardS1(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderBillboardS2(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderBillboardSV21(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderBillboardSV22(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

	void renderMetaball(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderControlBalls(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderBalls(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderAnimation(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

	void renderSort(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderInitCList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderNonZeroPrefix(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderCompactCList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderLengthCList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderInitHList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderSortCList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderBeginHList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderLengthHList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

	void renderPrefixSum(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderPrefixSumV2(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderEnvironment(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderControlMesh(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderAnimatedControlMesh(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderAnimatedControlMeshInTPose(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderFlatControlMesh(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderFlatAnimatedControlMesh(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderPBD(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void fillControlParticles(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void setBufferForIndirectDispatch(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void setAdaptiveControlPressure(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void rigControlParticles(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void animateControlParticles(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void stepAnimationKey(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

GG_ENDCLASS
