#include "Egg/Common.h"
#include "DualQuaternion.h"

DualQuaternion::DualQuaternion(const Egg11::Math::float4& orientation, const Egg11::Math::float4& translation)
{
	set(orientation, translation);
}

void DualQuaternion::set(const Egg11::Math::float4& orientation, const Egg11::Math::float4& translation)
{
	this->orientation = orientation;
	const Egg11::Math::float4& q0 = orientation;
	const Egg11::Math::float4& t = translation;
	Egg11::Math::float4& dq = this->translation;
	dq[0] = 0.5*( t[0]*q0[3] + t[1]*q0[2] - t[2]*q0[1]);
	dq[1] = 0.5*(-t[0]*q0[2] + t[1]*q0[3] + t[2]*q0[0]);
	dq[2] = 0.5*( t[0]*q0[1] - t[1]*q0[0] + t[2]*q0[3]);
	dq[3] =-0.5*( t[0]*q0[0] + t[1]*q0[1] + t[2]*q0[2]);

	/*dq_t qr = {orientation.w, orientation.x, orientation.y, orientation.z,
	 0, 0, 0, 0};
	double tt[3] = { translation.x, translation.y, translation.z };
	dq_t qt;
	dq_cr_translation(qt, 1.0, tt);
	dq_t pq;
	dq_op_mul(pq, qt, qr);
	bool k = true;*/
}

DualQuaternion DualQuaternion::operator*(const DualQuaternion & other) const
{
	DualQuaternion dq;
	dq.orientation = this->orientation.quatMul(other.orientation);
	dq.translation = this->orientation.quatMul(other.translation) + this->translation.quatMul(other.orientation);
	return dq;
}
