
struct PosDeltas4 {
	float3 d[4];
};

struct PosDeltas3 {
	float3 d[3];
};

PosDeltas4 EmptyPosDeltas4 () {
	PosDeltas4 result;
	result.d[0] = float3 (0.0, 0.0, 0.0);
	result.d[1] = float3 (0.0, 0.0, 0.0);
	result.d[2] = float3 (0.0, 0.0, 0.0);
	result.d[3] = float3 (0.0, 0.0, 0.0);
	return result;
}
/*
PosDeltas3 EmptyPosDeltas3() {
	PosDeltas3 result;
	result.d[0] = float3 (0.0, 0.0, 0.0);
	result.d[1] = float3 (0.0, 0.0, 0.0);
	result.d[2] = float3 (0.0, 0.0, 0.0);
	return result;
}
*/

float squaredLength(float3 v) {
	return dot(v, v);
}

PosDeltas4 get_nabla_p_Sij(const float4x4 F, const float4x4 C, uint i, uint j) {
	PosDeltas4 result = EmptyPosDeltas4 ();

	result.d[0] = float3(0.0f, 0.0f, 0.0f);
	for (uint pIdx = 1; pIdx < 4; pIdx++) {
		result.d[pIdx] =
			float3(F[0][j] * C[pIdx - 1][i], F[1][j] * C[pIdx - 1][i], F[2][j] * C[pIdx - 1][i]) +
			float3(F[0][i] * C[pIdx - 1][j], F[1][i] * C[pIdx - 1][j], F[2][i] * C[pIdx - 1][j]);
		result.d[0] -= result.d[pIdx];
	}

	return result;
}

PosDeltas4 get_overline_nabla_p_Sij(const float4x4 F, const float4x4 C, float Sij, uint i, uint j) {
	PosDeltas4 result = get_nabla_p_Sij(F, C, i, j);

	result.d[0] = float3(0.0f, 0.0f, 0.0f);
	for (uint pIdx = 1; pIdx < 4; pIdx++) {
		
		float3 iRankOne = float3(F[0][i] * C[pIdx - 1][i], F[1][i] * C[pIdx - 1][i], F[2][i] * C[pIdx - 1][i]);
		float3 jRankOne = float3(F[0][j] * C[pIdx - 1][j], F[1][j] * C[pIdx - 1][j], F[2][j] * C[pIdx - 1][j]);
		
		float iLengthPow2 = squaredLength(float3(F[0][i], F[1][i], F[2][i]));
		float jLengthPow2 = squaredLength(float3(F[0][j], F[1][j], F[2][j]));

		float iLength = sqrt(iLengthPow2);
		float jLength = sqrt(jLengthPow2);

		float iLengthPow3 = iLengthPow2 * iLength;
		float jLengthPow3 = jLengthPow2 * jLength;

		result.d[pIdx] /= iLength;
		result.d[pIdx] /= jLength;

		result.d[pIdx] -= ((iRankOne * iLengthPow2 + jRankOne * jLengthPow2) * Sij / iLengthPow3 / jLengthPow3);

		result.d[0] -= result.d[pIdx];
		
	}

	return result;
}

float get_lambda_stretch(float Sii, float sum_length_of_nabla_p_Sii) {
	float sqrtSii = sqrt(Sii);
	return 2.0f * sqrtSii * (sqrtSii - 1.0f) / (sum_length_of_nabla_p_Sii);
}

float get_lambda_shear(float Sij, float sum_length_of_nabla_p_Sij) {
	return Sij / (sum_length_of_nabla_p_Sij);
}

float get_lambda_volume(float Cvol, float sum_length_of_nabla_Cvol) {
	return Cvol / (sum_length_of_nabla_Cvol);
}

PosDeltas4 get_delta_p_for_stretch(const float4x4 F, const float4x4 C, const float Sii, uint i) {
	PosDeltas4 nabla_p_Sii = get_nabla_p_Sij(F, C, i, i);

	float sum_length = 0.0f;
	for (uint it = 0; it < 4; it++) {
		sum_length += squaredLength(nabla_p_Sii.d[it]);
	}

	for (uint k = 0; k < 4; k++) {
		nabla_p_Sii.d[k] *= -1.0f * get_lambda_stretch(Sii, sum_length);
	}

	return nabla_p_Sii;
}

PosDeltas4 get_delta_p_for_stretch(const float4x4 F, const float4x4 C, const float4x4 S) {
	PosDeltas4 delta_p = EmptyPosDeltas4();;

	for (uint i = 0; i < 3; i++) {
		PosDeltas4 delta_p_by_i = get_delta_p_for_stretch(F, C, S[i][i], i);
		for (uint k = 0; k < 4; k++) {
			delta_p.d[k] += delta_p_by_i.d[k];
		}
	}

	return delta_p;
}

PosDeltas4 get_delta_p_for_shear(const float4x4 F, const float4x4 C, const float Sij, uint i, uint j) {
	PosDeltas4 nabla_p_Sij = get_overline_nabla_p_Sij(F, C, Sij, i, j);

	float sum_length = 0.0f;
	for (uint it = 0; it < 4; it++) {
		sum_length += squaredLength(nabla_p_Sij.d[it]);
	}

	for (uint k = 0; k < 4; k++) {
		nabla_p_Sij.d[k] *= -1.0f * get_lambda_shear(Sij, sum_length);
	}

	return nabla_p_Sij;
}

PosDeltas4 add_by_k(PosDeltas4 prev, uint i, uint j, const float4x4 F, const float4x4 C, const float4x4 S) {
	PosDeltas4 delta_p_by_ij = get_delta_p_for_shear(F, C, S[i][j], i, j);
	for (uint k = 0; k < 4; k++)
	{
		prev.d[k] += delta_p_by_ij.d[k];
	}
	return prev;
}

PosDeltas4 get_delta_p_for_shear(const float4x4 F, const float4x4 C, const float4x4 S) {
	PosDeltas4 delta_p = EmptyPosDeltas4();

	delta_p  = add_by_k(delta_p, 1, 0, F, C, S);
	delta_p = add_by_k(delta_p, 2, 0, F, C, S);
	delta_p = add_by_k(delta_p, 2, 1, F, C, S);

	return delta_p;
}

PosDeltas4 get_delta_p_for_volume(const float4x4 P, const float4x4 Q) {
	PosDeltas4 delta_p = EmptyPosDeltas4();;

	//PosDeltas3 p = { float3{ P[0][0], P[1][0] , P[2][0] }, float3{ P[0][1], P[1][1] , P[2][1] }, float3{ P[0][2], P[1][2] , P[2][2] } };
	//PosDeltas3 q = { float3{ Q[0][0], P[1][0] , Q[2][0] }, float3{ Q[0][1], Q[1][1] , Q[2][1] }, float3{ Q[0][2], Q[1][2] , Q[2][2] } };
	PosDeltas3 p;
	p.d[0] = float3(P[0][0], P[1][0], P[2][0]);
	p.d[1] = float3(P[0][1], P[1][1], P[2][1]);
	p.d[2] = float3(P[0][2], P[1][2], P[2][2]);

	PosDeltas3 q;
	q.d[0] = float3(Q[0][0], P[1][0], Q[2][0]);
	q.d[1] = float3(Q[0][1], Q[1][1], Q[2][1]);
	q.d[2] = float3(Q[0][2], Q[1][2], Q[2][2]);

	//nabla p only at first
	delta_p.d[1] = cross(p.d[1], p.d[2]);
	delta_p.d[2] = cross(p.d[2], p.d[0]);
	delta_p.d[3] = cross(p.d[0], p.d[1]);
	delta_p.d[0] = -delta_p.d[1] - delta_p.d[2] - delta_p.d[3];

	float sum_length = 0.0f;
	for (uint it = 0; it < 4; it++) {
		sum_length += squaredLength (delta_p.d[it]);
	}

	float Cvol = dot (p.d[0], cross(p.d[1], p.d[2])) - dot (q.d[0], cross(q.d[1], q.d[2]));
	for (uint k = 0; k < 4; k++) {
		delta_p.d[k] *= -1.0f * get_lambda_volume(Cvol, sum_length);
	}

	return delta_p;
}

//Buffer<uint> controlParticleCounter;
StructuredBuffer<float4> defPos;
RWStructuredBuffer<float4> newPos;

float4x4 inverse(float4x4 m) {
	float n11 = m[0][0], n12 = m[1][0], n13 = m[2][0], n14 = m[3][0];
	float n21 = m[0][1], n22 = m[1][1], n23 = m[2][1], n24 = m[3][1];
	float n31 = m[0][2], n32 = m[1][2], n33 = m[2][2], n34 = m[3][2];
	float n41 = m[0][3], n42 = m[1][3], n43 = m[2][3], n44 = m[3][3];

	float t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44;
	float t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44;
	float t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44;
	float t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;

	float det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;
	float idet = 1.0f / det;

	float4x4 ret;

	ret[0][0] = t11 * idet;
	ret[0][1] = (n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44) * idet;
	ret[0][2] = (n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44) * idet;
	ret[0][3] = (n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43) * idet;

	ret[1][0] = t12 * idet;
	ret[1][1] = (n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44) * idet;
	ret[1][2] = (n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44) * idet;
	ret[1][3] = (n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43) * idet;

	ret[2][0] = t13 * idet;
	ret[2][1] = (n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44) * idet;
	ret[2][2] = (n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44) * idet;
	ret[2][3] = (n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43) * idet;

	ret[3][0] = t14 * idet;
	ret[3][1] = (n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34) * idet;
	ret[3][2] = (n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34) * idet;
	ret[3][3] = (n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33) * idet;

	return ret;
}


cbuffer PBDTetrahedronCB {
	float4x4 C;
};


void executeConstraintsOnVertices(uint4 pIdx) {
	float4x4 P = float4x4
		(
			newPos[pIdx[1]].x - newPos[pIdx[0]].x, newPos[pIdx[2]].x - newPos[pIdx[0]].x, newPos[pIdx[3]].x - newPos[pIdx[0]].x, 0.0f,
			newPos[pIdx[1]].y - newPos[pIdx[0]].y, newPos[pIdx[2]].y - newPos[pIdx[0]].y, newPos[pIdx[3]].y - newPos[pIdx[0]].y, 0.0f,
			newPos[pIdx[1]].z - newPos[pIdx[0]].z, newPos[pIdx[2]].z - newPos[pIdx[0]].z, newPos[pIdx[3]].z - newPos[pIdx[0]].z, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		);

	
	float4x4 Q = float4x4
		(
			defPos[pIdx[1]].x - defPos[pIdx[0]].x, defPos[pIdx[2]].x - defPos[pIdx[0]].x, defPos[pIdx[3]].x - defPos[pIdx[0]].x, 0.0f,
			defPos[pIdx[1]].y - defPos[pIdx[0]].y, defPos[pIdx[2]].y - defPos[pIdx[0]].y, defPos[pIdx[3]].y - defPos[pIdx[0]].y, 0.0f,
			defPos[pIdx[1]].z - defPos[pIdx[0]].z, defPos[pIdx[2]].z - defPos[pIdx[0]].z, defPos[pIdx[3]].z - defPos[pIdx[0]].z, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
			);
	
	//float4x4 C = inverse(Q);
	//float4x4 C = float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
	//P = C;
	//Q = C;

	//float4x4 F = P * C;
	//float4x4 S = transpose(F) * F;
	float4x4 F = mul(P, C);
	float4x4 S = mul(transpose(F), F);

	//Stretch
	PosDeltas4 deltaPforStretch = get_delta_p_for_stretch(F, C, S);
	for (uint k = 0; k < 4; k++) {
		newPos[pIdx[k]].xyz += deltaPforStretch.d[k] * 0.03;
	}

	//Shear
	PosDeltas4 deltaPforShear = get_delta_p_for_shear(F, C, S);
	for (uint k = 0; k < 4; k++) {
		newPos[pIdx[k]].xyz += deltaPforShear.d[k] * 0.02;
	}

	//Volume
	PosDeltas4 deltaPforVolume = get_delta_p_for_volume(P, Q);
	for (uint k = 0; k < 4; k++) {
		newPos[pIdx[k]].xyz += deltaPforVolume.d[k] * 0.03;
	}
}

uint changeToArrayIndex(uint x, uint y, uint z, uint n) {
	return n * gridSize * gridSize * gridSize + z  * gridSize * gridSize + y * gridSize + x;
}