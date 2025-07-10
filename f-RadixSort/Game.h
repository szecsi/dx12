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

	static const unsigned int windowHeight = 512;
	static const unsigned int windowWidth = 512;

	static const unsigned int fillWindowHeight = 200;
	static const unsigned int fillWindowWidth = 200;

	static const unsigned int counterSize = 3;

private:
	enum BillboardsAlgorithm { Normal, ABuffer, SBuffer, SBufferV2, HashSimple };
	enum RenderMode { Realistic, Gradient, ControlParticles, Particles };
	enum FlowControl { RealisticFlow, ControlledFlow };
	enum ControlParticlePlacement { Vertex, Render, Animated, PBD, CPU };
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
	std::vector<ControlParticle> controlParticles;

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
	Microsoft::WRL::ComPtr<ID3D11Buffer> particlePositionBuffer[2];
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particlePositionSRV[2];
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particlePositionUAV[2];
	Microsoft::WRL::ComPtr<ID3D11Buffer> particleVelocityBuffer[2];
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleVelocitySRV[2];
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particleVelocityUAV[2];
	Microsoft::WRL::ComPtr<ID3D11Buffer> particleMassDensityBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleMassDensitySRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particleMassDensityUAV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> particlePressureBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particlePressureSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particlePressureUAV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> particleForceBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleForceSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particleFroceUAV;
//to12	Microsoft::WRL::ComPtr<ID3D11Buffer> particleHashBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleHashSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particleHashUAV; // view on 12 resource
	Microsoft::WRL::ComPtr<ID3D11Buffer> particleFrictionBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleFrictionSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particleFrictionUAV;

	// Control Particle
	Microsoft::WRL::ComPtr<ID3D11Buffer> controlParticleDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> controlParticleSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> controlParticleUAV;

	/*
	float3	position;
	float	controlPressureRatio;
	float3	nonAnimatedPos;
	float	temp;
	float4	blendWeights;
	uint4	blendIndices;
	*/
	Microsoft::WRL::ComPtr<ID3D11Buffer> controlParticlePositionBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> controlParticlePositionSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> controlParticlePositionUAV;

	Microsoft::WRL::ComPtr<ID3D11Buffer> controlParticleDefaultPositionBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> controlParticleDefaultPositionSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> controlParticleDefaultPositionUAV;

	Microsoft::WRL::ComPtr<ID3D11Buffer> controlParticleBlendWeightBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> controlParticleBlendWeightSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> controlParticleBlendWeightUAV;

	Microsoft::WRL::ComPtr<ID3D11Buffer> controlParticleBlendIndexBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> controlParticleBlendIndexSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> controlParticleBlendIndexUAV;

	Microsoft::WRL::ComPtr<ID3D11Buffer> controlParticlePressureRatioBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> controlParticlePressureRatioSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> controlParticlePressureRatioUAV;

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
	Microsoft::WRL::ComPtr<ID3D11Buffer> controlParticleDefPosDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> controlParticleDefPosSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> controlParticleDefPosUAV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> controlParticleNewPosDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> controlParticleNewPosSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> controlParticleNewPosUAV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> controlParticleVelocityDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> controlParticleVelocitySRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> controlParticleVelocityUAV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> CmatCB;
	Egg11::Mesh::Shaded::P PBDTestMesh;
	Microsoft::WRL::ComPtr<ID3D11Buffer> PBDTestMeshPosDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  PBDTestMeshPosSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>  PBDTestMeshPosUAV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> PBDTestMeshTransDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  PBDTestMeshTransSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>  PBDTestMeshTransUAV;
	Egg11::Mesh::Shader::P PBDShaderGravity;
	Egg11::Mesh::Shader::P PBDShaderCollision;
	Egg11::Mesh::Shader::P PBDShaderDistance;
	Egg11::Mesh::Shader::P PBDShaderBending;
	Egg11::Mesh::Shader::P PBDShaderFinalUpdate;
	std::array<Egg11::Mesh::Shader::P, 26> PBDShaderTetrahedron;
	Egg11::Mesh::Shader::P PBDShaderSetDefPos;
	Egg11::Mesh::Shader::P PBDShaderSphereCollision;
	Egg11::Mesh::Shader::P PBDShaderSphereAnimate;
	Egg11::Mesh::Shader::P PBDShaderSphereTransClear;
	Egg11::Mesh::Shader::P PBDShaderVelocityFilter;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> solidRenderTargetTexture;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>solidRenderTargetView;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> solidShaderResourceView;

	// Hashtables
//to12	Microsoft::WRL::ComPtr<ID3D11Buffer> clistDataBuffer;
//to12	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> clistSRV;
//to12	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> clistUAV;
//to12	Microsoft::WRL::ComPtr<ID3D11Buffer> clistLengthDataBuffer;
//to12	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> clistLengthSRV;
//to12	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> clistLengthUAV;
//to12	Microsoft::WRL::ComPtr<ID3D11Buffer> clistBeginDataBuffer;
//to12	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> clistBeginSRV;
//to12	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> clistBeginUAV;
//to12	Microsoft::WRL::ComPtr<ID3D11Buffer> clistCellCountDataBuffer;
//to12	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> clistCellCountSRV;
//to12	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> clistCellCountUAV;
//to12
//to12	Microsoft::WRL::ComPtr<ID3D11Buffer> hlistDataBuffer;
//to12	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> hlistSRV;
//to12	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> hlistUAV;
//to12	Microsoft::WRL::ComPtr<ID3D11Buffer> hlistLengthDataBuffer;
//to12	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> hlistLengthSRV;
//to12	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> hlistLengthUAV;
//to12	Microsoft::WRL::ComPtr<ID3D11Buffer> hlistBeginDataBuffer;
//to12	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> hlistBeginSRV;
//to12	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> hlistBeginUAV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> sortedParticleIndicesUAV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> cellLutUAV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> hashLutUAV;

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

	// Sponge
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spongeDiffuseSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spongeNormalSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spongeHeightSRV;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState2;

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
	Egg11::Mesh::Shader::P fluidSimulationMassPressShader;
	Egg11::Mesh::Shader::P fluidSimulationForcesShader;
	Egg11::Mesh::Shader::P fluidSimulationForcesControlledShader;
	Egg11::Mesh::Shader::P fluidSimulationFinalShader;

	Egg11::Mesh::Shader::P sortParticlesShader;
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

	// Sponge
	Egg11::Mesh::Shaded::P spongeMesh;

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


	Microsoft::WRL::ComPtr<ID3D11Buffer> mortonsBuffer;

public:
	Game(Microsoft::WRL::ComPtr<ID3D11Device2> device);
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
	void CreateSpongeMesh();
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
	void clearSolidRenderTarget(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
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

	void renderSpongeMesh(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
//to12	void renderSort(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
//to12	void renderInitCList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
//to12	void renderNonZeroPrefix(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
//to12	void renderCompactCList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
//to12	void renderLengthCList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
//to12	void renderInitHList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
//to12	void renderSortCList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
//to12	void renderBeginHList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
//to12	void renderLengthHList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

	void renderPrefixSum(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderPrefixSumV2(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderEnvironment(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderControlMesh(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderAnimatedControlMesh(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderAnimatedControlMeshInTPose(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderFlatControlMesh(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderFlatAnimatedControlMesh(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderPBD(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderPBDOnCPU(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void fillControlParticles(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void setBufferForIndirectDispatch(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void setAdaptiveControlPressure(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void rigControlParticles(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void animateControlParticles(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void stepAnimationKey(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	void renderTestMesh(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

	Egg11::Math::float3 calculateNormal(Egg11::Math::float3 p0, Egg11::Math::float3 p1, Egg11::Math::float3 p2);
	Egg11::Math::float3 calculateBinormal(Egg11::Math::float3 p0, Egg11::Math::float3 p1, Egg11::Math::float3 p2, Egg11::Math::float2 t0, Egg11::Math::float2 t1, Egg11::Math::float2 t2);
	Egg11::Math::float3 calculateTangent(Egg11::Math::float3 p0, Egg11::Math::float3 p1, Egg11::Math::float3 p2, Egg11::Math::float2 t0, Egg11::Math::float2 t1, Egg11::Math::float2 t2);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> loadTexture(std::string name);

GG_ENDCLASS
