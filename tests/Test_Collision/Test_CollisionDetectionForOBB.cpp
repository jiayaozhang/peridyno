#include "gtest/gtest.h"

#include "Quat.h"
#include "Topology/Primitive3D.h"

using namespace dyno;

using Quat1f = Quat<float>;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat3 = glm::mat3;

typedef signed int i32;
typedef float r32;
typedef unsigned char u8;

#define Q3_R32_MAX 1000000;

vec3 quat_rotate(vec4 quat, vec3 v)
{
	// Extract the vector part of the quaternion
	vec3 u = vec3(quat.x, quat.y, quat.z);

	// Extract the scalar part of the quaternion
	float s = quat.w;

	// Do the math
	return    2.0f * dot(u, v) * u
		+ (s*s - dot(u, u)) * v
		+ 2.0f * s * cross(u, v);
}

float fsign(float v)
{
	return v < 0 ? -1.0f : 1.0f;
}

mat3 quat_to_mat3(vec4 quat)
{
	float x2 = quat.x + quat.x;
	float y2 = quat.y + quat.y;
	float z2 = quat.z + quat.z;
	float xx = x2 * quat.x;
	float yy = y2 * quat.y;
	float zz = z2 * quat.z;
	float xy = x2 * quat.y;
	float xz = x2 * quat.z;
	float xw = x2 * quat.w;
	float yz = y2 * quat.z, yw = y2 * quat.w, zw = z2 * quat.w;
	return mat3(1.0 - yy - zz, xy - zw, xz + yw,
		xy + zw, 1.0 - xx - zz, yz - xw,
		xz - yw, yz + xw, 1.0 - xx - yy);
}

union q3FeaturePair
{
	struct
	{
		u8 inR;
		u8 outR;
		u8 inI;
		u8 outI;
	};

	i32 key;
};

struct Transform
{
	vec3 position;
	mat3 rotation;
};

struct q3Contact
{
	vec3 position;			// World coordinate of contact
	r32 penetration;			// Depth of penetration from collision
};

struct q3Manifold
{
	vec3 normal;				// From A to B
	q3Contact contacts[8];
	i32 contactCount;
};

struct Box
{
	Box()
	{
		center = vec3(0.0f, 0.0f, 0.0f);
		halfLength = vec3(1.0f, 1.0f, 1.0f);
		rot = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	vec3 center;
	vec3 halfLength;

	vec4 rot;
};

//--------------------------------------------------------------------------------------------------
inline const vec3 q3Mul(const mat3& r, const vec3& v)
{
	return r * v;
}

inline const vec3 q3Mul(const Transform& tx, const vec3& v)
{
	return vec3(tx.rotation * v + tx.position);
}

inline const vec3 q3MulT(const Transform& tx, const vec3& v)
{
	return transpose(tx.rotation) * (v - tx.position);
}

//--------------------------------------------------------------------------------------------------
inline const vec3 q3MulT(const mat3& r, const vec3& v)
{
	return transpose(r) * v;
}

inline const vec3 q3Mul(const mat3& rot, const vec3& trans, const vec3& v)
{
	return vec3(rot * v + trans);
}


//--------------------------------------------------------------------------------------------------
// qBoxtoBox
//--------------------------------------------------------------------------------------------------
inline bool q3TrackFaceAxis(i32* axis, i32 n, r32 s, r32* sMax, const vec3& normal, vec3* axisNormal)
{
	if (s > r32(0.0))
		return true;

	if (s > *sMax)
	{
		*sMax = s;
		*axis = n;
		*axisNormal = normal;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------
inline bool q3TrackEdgeAxis(i32* axis, i32 n, r32 s, r32* sMax, const vec3& normal, vec3* axisNormal)
{
	if (s > r32(0.0))
		return true;

	r32 l = r32(1.0) / length(normal);
	s *= l;

	if (s > *sMax)
	{
		*sMax = s;
		*axis = n;
		*axisNormal = normal * l;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------
struct q3ClipVertex
{
	q3ClipVertex()
	{
		f.key = ~0;
	}

	vec3 v;
	q3FeaturePair f;
};

//--------------------------------------------------------------------------------------------------
void q3ComputeReferenceEdgesAndBasis(const vec3& eR, const Transform& rtx, vec3 n, i32 axis, u8* out, mat3* basis, vec3* e)
{
	n = q3MulT(rtx.rotation, n);

	if (axis >= 3)
		axis -= 3;

	mat3 rT = rtx.rotation;
	mat3 outB;

	switch (axis)
	{
	case 0:
		if (n.x > r32(0.0))
		{
			out[0] = 1;
			out[1] = 8;
			out[2] = 7;
			out[3] = 9;

			*e = vec3(eR.y, eR.z, eR.x);
			//basis->SetRows(rtx.rotation.ey, rtx.rotation.ez, rtx.rotation.ex);
			outB[0] = rT[1];
			outB[1] = rT[2];
			outB[2] = rT[0];
		}
		else
		{
			out[0] = 11;
			out[1] = 3;
			out[2] = 10;
			out[3] = 5;

			*e = vec3(eR.z, eR.y, eR.x);
			//basis->SetRows(rtx.rotation.ez, rtx.rotation.ey, -rtx.rotation.ex);
			outB[0] = rT[2];
			outB[1] = rT[1];
			outB[2] = -rT[0];
		}
		break;

	case 1:
		if (n.y > r32(0.0))
		{
			out[0] = 0;
			out[1] = 1;
			out[2] = 2;
			out[3] = 3;

			*e = vec3(eR.z, eR.x, eR.y);
			//basis->SetRows(rtx.rotation.ez, rtx.rotation.ex, rtx.rotation.ey);
			outB[0] = rT[2];
			outB[1] = rT[0];
			outB[2] = rT[1];
		}
		else
		{
			out[0] = 4;
			out[1] = 5;
			out[2] = 6;
			out[3] = 7;

			*e = vec3(eR.z, eR.x, eR.y);
			//basis->SetRows(rtx.rotation.ez, -rtx.rotation.ex, -rtx.rotation.ey);
			outB[0] = rT[2];
			outB[1] = -rT[0];
			outB[2] = -rT[1];
		}
		break;

	case 2:
		if (n.z > r32(0.0))
		{
			out[0] = 11;
			out[1] = 4;
			out[2] = 8;
			out[3] = 0;

			*e = vec3(eR.y, eR.x, eR.z);
			//basis->SetRows(-rtx.rotation.ey, rtx.rotation.ex, rtx.rotation.ez);
			outB[0] = -rT[1];
			outB[1] = rT[0];
			outB[2] = rT[2];
		}
		else
		{
			out[0] = 6;
			out[1] = 10;
			out[2] = 2;
			out[3] = 9;

			*e = vec3(eR.y, eR.x, eR.z);
			//basis->SetRows(-rtx.rotation.ey, -rtx.rotation.ex, -rtx.rotation.ez);
			outB[0] = -rT[1];
			outB[1] = -rT[0];
			outB[2] = -rT[2];
		}
		break;
	}

	*basis = outB;
}

//--------------------------------------------------------------------------------------------------
void q3ComputeIncidentFace(const Transform& itx, const vec3& e, vec3 n, q3ClipVertex* out)
{
	n = -q3MulT(itx.rotation, n);
	vec3 absN = abs(n);

	if (absN.x > absN.y && absN.x > absN.z)
	{
		if (n.x > r32(0.0))
		{
			out[0].v = vec3(e.x, e.y, -e.z);
			out[1].v = vec3(e.x, e.y, e.z);
			out[2].v = vec3(e.x, -e.y, e.z);
			out[3].v = vec3(e.x, -e.y, -e.z);

			out[0].f.inI = 9;
			out[0].f.outI = 1;
			out[1].f.inI = 1;
			out[1].f.outI = 8;
			out[2].f.inI = 8;
			out[2].f.outI = 7;
			out[3].f.inI = 7;
			out[3].f.outI = 9;
		}
		else
		{
			out[0].v = vec3(-e.x, -e.y, e.z);
			out[1].v = vec3(-e.x, e.y, e.z);
			out[2].v = vec3(-e.x, e.y, -e.z);
			out[3].v = vec3(-e.x, -e.y, -e.z);

			out[0].f.inI = 5;
			out[0].f.outI = 11;
			out[1].f.inI = 11;
			out[1].f.outI = 3;
			out[2].f.inI = 3;
			out[2].f.outI = 10;
			out[3].f.inI = 10;
			out[3].f.outI = 5;
		}
	}
	else if (absN.y > absN.x && absN.y > absN.z)
	{
		if (n.y > r32(0.0))
		{
			out[0].v = vec3(-e.x, e.y, e.z);
			out[1].v = vec3(e.x, e.y, e.z);
			out[2].v = vec3(e.x, e.y, -e.z);
			out[3].v = vec3(-e.x, e.y, -e.z);

			out[0].f.inI = 3;
			out[0].f.outI = 0;
			out[1].f.inI = 0;
			out[1].f.outI = 1;
			out[2].f.inI = 1;
			out[2].f.outI = 2;
			out[3].f.inI = 2;
			out[3].f.outI = 3;
		}
		else
		{
			out[0].v = vec3(e.x, -e.y, e.z);
			out[1].v = vec3(-e.x, -e.y, e.z);
			out[2].v = vec3(-e.x, -e.y, -e.z);
			out[3].v = vec3(e.x, -e.y, -e.z);

			out[0].f.inI = 7;
			out[0].f.outI = 4;
			out[1].f.inI = 4;
			out[1].f.outI = 5;
			out[2].f.inI = 5;
			out[2].f.outI = 6;
			out[3].f.inI = 5;
			out[3].f.outI = 6;
		}
	}

	else
	{
		if (n.z > r32(0.0))
		{
			out[0].v = vec3(-e.x, e.y, e.z);
			out[1].v = vec3(-e.x, -e.y, e.z);
			out[2].v = vec3(e.x, -e.y, e.z);
			out[3].v = vec3(e.x, e.y, e.z);

			out[0].f.inI = 0;
			out[0].f.outI = 11;
			out[1].f.inI = 11;
			out[1].f.outI = 4;
			out[2].f.inI = 4;
			out[2].f.outI = 8;
			out[3].f.inI = 8;
			out[3].f.outI = 0;
		}

		else
		{
			out[0].v = vec3(e.x, -e.y, -e.z);
			out[1].v = vec3(-e.x, -e.y, -e.z);
			out[2].v = vec3(-e.x, e.y, -e.z);
			out[3].v = vec3(e.x, e.y, -e.z);

			out[0].f.inI = 9;
			out[0].f.outI = 6;
			out[1].f.inI = 6;
			out[1].f.outI = 10;
			out[2].f.inI = 10;
			out[2].f.outI = 2;
			out[3].f.inI = 2;
			out[3].f.outI = 9;
		}
	}

	for (i32 i = 0; i < 4; ++i)
		out[i].v = q3Mul(itx, out[i].v);
}

//--------------------------------------------------------------------------------------------------
#define InFront( a ) \
	((a) < r32( 0.0 ))

#define Behind( a ) \
	((a) >= r32( 0.0 ))

#define On( a ) \
	((a) < r32( 0.005 ) && (a) > -r32( 0.005 ))

i32 q3Orthographic(r32 sign, r32 e, i32 axis, i32 clipEdge, q3ClipVertex* in, i32 inCount, q3ClipVertex* out)
{
	i32 outCount = 0;
	q3ClipVertex a = in[inCount - 1];

	for (i32 i = 0; i < inCount; ++i)
	{
		q3ClipVertex b = in[i];

		r32 da = sign * a.v[axis] - e;
		r32 db = sign * b.v[axis] - e;

		q3ClipVertex cv;

		// B
		if (((InFront(da) && InFront(db)) || On(da) || On(db)))
		{
			assert(outCount < 8);
			out[outCount++] = b;
		}

		// I
		else if (InFront(da) && Behind(db))
		{
			cv.f = b.f;
			cv.v = a.v + (b.v - a.v) * (da / (da - db));
			cv.f.outR = clipEdge;
			cv.f.outI = 0;
			assert(outCount < 8);
			out[outCount++] = cv;
		}

		// I, B
		else if (Behind(da) && InFront(db))
		{
			cv.f = a.f;
			cv.v = a.v + (b.v - a.v) * (da / (da - db));
			cv.f.inR = clipEdge;
			cv.f.inI = 0;
			assert(outCount < 8);
			out[outCount++] = cv;

			assert(outCount < 8);
			out[outCount++] = b;
		}

		a = b;
	}

	return outCount;
}

//--------------------------------------------------------------------------------------------------
// Resources (also see q3BoxtoBox's resources):
// http://www.randygaul.net/2013/10/27/sutherland-hodgman-clipping/
i32 q3Clip(const vec3& rPos, const vec3& e, u8* clipEdges, const mat3& basis, q3ClipVertex* incident, q3ClipVertex* outVerts, r32* outDepths)
{
	i32 inCount = 4;
	i32 outCount;
	q3ClipVertex in[8];
	q3ClipVertex out[8];

	for (i32 i = 0; i < 4; ++i)
		in[i].v = q3MulT(basis, incident[i].v - rPos);

	outCount = q3Orthographic(r32(1.0), e.x, 0, clipEdges[0], in, inCount, out);

	if (!outCount)
		return 0;

	inCount = q3Orthographic(r32(1.0), e.y, 1, clipEdges[1], out, outCount, in);

	if (!inCount)
		return 0;

	outCount = q3Orthographic(r32(-1.0), e.x, 0, clipEdges[2], in, inCount, out);

	if (!outCount)
		return 0;

	inCount = q3Orthographic(r32(-1.0), e.y, 1, clipEdges[3], out, outCount, in);

	// Keep incident vertices behind the reference face
	outCount = 0;
	for (i32 i = 0; i < inCount; ++i)
	{
		r32 d = in[i].v.z - e.z;

		if (d <= r32(0.0))
		{
			outVerts[outCount].v = q3Mul(basis, in[i].v) + rPos;
			outVerts[outCount].f = in[i].f;
			outDepths[outCount++] = d;
		}
	}

	assert(outCount <= 8);

	return outCount;
}

//--------------------------------------------------------------------------------------------------
inline void q3EdgesContact(vec3 *CA, vec3 *CB, const vec3& PA, const vec3& QA, const vec3& PB, const vec3& QB)
{
	vec3 DA = QA - PA;
	vec3 DB = QB - PB;
	vec3 r = PA - PB;
	r32 a = dot(DA, DA);
	r32 e = dot(DB, DB);
	r32 f = dot(DB, r);
	r32 c = dot(DA, r);

	r32 b = dot(DA, DB);
	r32 denom = a * e - b * b;

	r32 TA = (b * f - c * e) / denom;
	r32 TB = (b * TA + f) / e;

	*CA = PA + DA * TA;
	*CB = PB + DB * TB;
}

//--------------------------------------------------------------------------------------------------
void q3SupportEdge(const mat3& rot, const vec3& trans, const vec3& e, vec3 n, vec3* aOut, vec3* bOut)
{
	n = q3MulT(rot, n);
	vec3 absN = abs(n);
	vec3 a, b;

	// x > y
	if (absN.x > absN.y)
	{
		// x > y > z
		if (absN.y > absN.z)
		{
			a = vec3(e.x, e.y, e.z);
			b = vec3(e.x, e.y, -e.z);
		}

		// x > z > y || z > x > y
		else
		{
			a = vec3(e.x, e.y, e.z);
			b = vec3(e.x, -e.y, e.z);
		}
	}

	// y > x
	else
	{
		// y > x > z
		if (absN.x > absN.z)
		{
			a = vec3(e.x, e.y, e.z);
			b = vec3(e.x, e.y, -e.z);
		}

		// z > y > x || y > z > x
		else
		{
			a = vec3(e.x, e.y, e.z);
			b = vec3(-e.x, e.y, e.z);
		}
	}

	r32 signx = fsign(n.x);
	r32 signy = fsign(n.y);
	r32 signz = fsign(n.z);

	a.x *= signx;
	a.y *= signy;
	a.z *= signz;
	b.x *= signx;
	b.y *= signy;
	b.z *= signz;

	*aOut = q3Mul(rot, trans, a);
	*bOut = q3Mul(rot, trans, b);
}

//--------------------------------------------------------------------------------------------------
// Resources:
// http://www.randygaul.net/2014/05/22/deriving-obb-to-obb-intersection-sat/
// https://box2d.googlecode.com/files/GDC2007_ErinCatto.zip
// https://box2d.googlecode.com/files/Box2D_Lite.zip
void q3BoxtoBox(q3Manifold& m, Box box0, Box box1)
{
	vec3 v = box1.center - box0.center;

	vec3 eA = box0.halfLength;
	vec3 eB = box1.halfLength;

	mat3 rotA = quat_to_mat3(box0.rot);
	mat3 rotB = quat_to_mat3(box1.rot);

	mat3 rotAT = transpose(rotA);
	mat3 rotBT = transpose(rotB);

	// B's frame in A's space
	mat3 C = transpose(rotA) * rotB;
	mat3 absC;
	bool parallel = false;
	const r32 kCosTol = r32(1.0e-6);
	for (i32 i = 0; i < 3; ++i)
	{
		for (i32 j = 0; j < 3; ++j)
		{
			r32 val = abs(C[i][j]);
			absC[i][j] = val;

			if (val + kCosTol >= r32(1.0))
				parallel = true;
		}
	}

	mat3 cT = transpose(C);
	mat3 absCT = transpose(absC);

	// Vector from center A to center B in A's space
	vec3 t = transpose(rotA) * v;

	// Query states
	r32 s;
	r32 aMax = -Q3_R32_MAX;
	r32 bMax = -Q3_R32_MAX;
	r32 eMax = -Q3_R32_MAX;
	i32 aAxis = ~0;
	i32 bAxis = ~0;
	i32 eAxis = ~0;
	vec3 nA;
	vec3 nB;
	vec3 nE;

	// Face axis checks

	// a's x axis
	s = abs(t.x) - (box0.halfLength.x + dot(absC[0], box1.halfLength));
	if (q3TrackFaceAxis(&aAxis, 0, s, &aMax, rotA[0], &nA))
		return;

	// a's y axis
	s = abs(t.y) - (box0.halfLength.y + dot(absC[1], box1.halfLength));
	if (q3TrackFaceAxis(&aAxis, 1, s, &aMax, rotA[1], &nA))
		return;

	// a's z axis
	s = abs(t.z) - (box0.halfLength.z + dot(absC[2], box1.halfLength));
	if (q3TrackFaceAxis(&aAxis, 2, s, &aMax, rotA[2], &nA))
		return;

	// b's x axis
	s = abs(dot(t, cT[0])) - (box1.halfLength.x + dot(absCT[0], box0.halfLength));
	if (q3TrackFaceAxis(&bAxis, 3, s, &bMax, rotB[0], &nB))
		return;

	// b's y axis
	s = abs(dot(t, cT[1])) - (box1.halfLength.y + dot(absCT[1], box0.halfLength));
	if (q3TrackFaceAxis(&bAxis, 4, s, &bMax, rotB[1], &nB))
		return;

	// b's z axis
	s = abs(dot(t, cT[2])) - (box1.halfLength.z + dot(absCT[2], box0.halfLength));
	if (q3TrackFaceAxis(&bAxis, 5, s, &bMax, rotB[2], &nB))
		return;

	if (!parallel)
	{
		// Edge axis checks
		r32 rA;
		r32 rB;

		// Cross( a.x, b.x )
		rA = eA.y * absC[0][2] + eA.z * absC[0][1];
		rB = eB.y * absC[2][0] + eB.z * absC[1][0];
		s = abs(t.z * C[0][1] - t.y * C[0][2]) - (rA + rB);
		if (q3TrackEdgeAxis(&eAxis, 6, s, &eMax, vec3(r32(0.0), -C[0][2], C[0][1]), &nE))
			return;

		// Cross( a.x, b.y )
		rA = eA.y * absC[1][2] + eA.z * absC[1][1];
		rB = eB.x * absC[2][0] + eB.z * absC[0][0];
		s = abs(t.z * C[1][1] - t.y * C[1][2]) - (rA + rB);
		if (q3TrackEdgeAxis(&eAxis, 7, s, &eMax, vec3(r32(0.0), -C[1][2], C[1][1]), &nE))
			return;

		// Cross( a.x, b.z )
		rA = eA.y * absC[2][2] + eA.z * absC[2][1];
		rB = eB.x * absC[1][0] + eB.y * absC[0][0];
		s = abs(t.z * C[2][1] - t.y * C[2][2]) - (rA + rB);
		if (q3TrackEdgeAxis(&eAxis, 8, s, &eMax, vec3(r32(0.0), -C[2][2], C[2][1]), &nE))
			return;

		// Cross( a.y, b.x )
		rA = eA.x * absC[0][2] + eA.z * absC[0][0];
		rB = eB.y * absC[2][1] + eB.z * absC[1][1];
		s = abs(t.x * C[0][2] - t.z * C[0][0]) - (rA + rB);
		if (q3TrackEdgeAxis(&eAxis, 9, s, &eMax, vec3(C[0][2], r32(0.0), -C[0][0]), &nE))
			return;

		// Cross( a.y, b.y )
		rA = eA.x * absC[1][2] + eA.z * absC[1][0];
		rB = eB.x * absC[2][1] + eB.z * absC[0][1];
		s = abs(t.x * C[1][2] - t.z * C[1][0]) - (rA + rB);
		if (q3TrackEdgeAxis(&eAxis, 10, s, &eMax, vec3(C[1][2], r32(0.0), -C[1][0]), &nE))
			return;

		// Cross( a.y, b.z )
		rA = eA.x * absC[2][2] + eA.z * absC[2][0];
		rB = eB.x * absC[1][1] + eB.y * absC[0][1];
		s = abs(t.x * C[2][2] - t.z * C[2][0]) - (rA + rB);
		if (q3TrackEdgeAxis(&eAxis, 11, s, &eMax, vec3(C[2][2], r32(0.0), -C[2][0]), &nE))
			return;

		// Cross( a.z, b.x )
		rA = eA.x * absC[0][1] + eA.y * absC[0][0];
		rB = eB.y * absC[2][2] + eB.z * absC[1][2];
		s = abs(t.y * C[0][0] - t.x * C[0][1]) - (rA + rB);
		if (q3TrackEdgeAxis(&eAxis, 12, s, &eMax, vec3(-C[0][1], C[0][0], r32(0.0)), &nE))
			return;

		// Cross( a.z, b.y )
		rA = eA.x * absC[1][1] + eA.y * absC[1][0];
		rB = eB.x * absC[2][2] + eB.z * absC[0][2];
		s = abs(t.y * C[1][0] - t.x * C[1][1]) - (rA + rB);
		if (q3TrackEdgeAxis(&eAxis, 13, s, &eMax, vec3(-C[1][1], C[1][0], r32(0.0)), &nE))
			return;

		// Cross( a.z, b.z )
		rA = eA.x * absC[2][1] + eA.y * absC[2][0];
		rB = eB.x * absC[1][2] + eB.y * absC[0][2];
		s = abs(t.y * C[2][0] - t.x * C[2][1]) - (rA + rB);
		if (q3TrackEdgeAxis(&eAxis, 14, s, &eMax, vec3(-C[2][1], C[2][0], r32(0.0)), &nE))
			return;
	}

	// Artificial axis bias to improve frame coherence
	const r32 kRelTol = r32(0.95);
	const r32 kAbsTol = r32(0.01);
	i32 axis;
	r32 sMax;
	vec3 n;
	r32 faceMax = std::max(aMax, bMax);
	if (kRelTol * eMax > faceMax + kAbsTol)
	{
		axis = eAxis;
		sMax = eMax;
		n = nE;
	}
	else
	{
		if (kRelTol * bMax > aMax + kAbsTol)
		{
			axis = bAxis;
			sMax = bMax;
			n = nB;
		}
		else
		{
			axis = aAxis;
			sMax = aMax;
			n = nA;
		}
	}

	if (dot(n, v) < r32(0.0))
		n = -n;

	if (axis == ~0)
		return;

	Transform atx;
	Transform btx;
	atx.position = box0.center;
	atx.rotation = rotA;

	btx.position = box1.center;
	btx.rotation = rotB;

	if (axis < 6)
	{
		Transform rtx;
		Transform itx;
		vec3 eR;
		vec3 eI;
		bool flip;

		if (axis < 3)
		{
			rtx = atx;
			itx = btx;
			eR = eA;
			eI = eB;
			flip = false;
		}

		else
		{
			rtx = btx;
			itx = atx;
			eR = eB;
			eI = eA;
			flip = true;
			n = -n;
		}

		// Compute reference and incident edge information necessary for clipping
		q3ClipVertex incident[4];
		q3ComputeIncidentFace(itx, eI, n, incident);
		u8 clipEdges[4];
		mat3 basis;
		vec3 e;
		q3ComputeReferenceEdgesAndBasis(eR, rtx, n, axis, clipEdges, &basis, &e);

		// Clip the incident face against the reference face side planes
		q3ClipVertex out[8];
		r32 depths[8];
		i32 outNum;
		outNum = q3Clip(rtx.position, e, clipEdges, basis, incident, out, depths);

		if (outNum)
		{
			m.contactCount = outNum;
			m.normal = flip ? -n : n;

			for (i32 i = 0; i < outNum; ++i)
			{
				q3Contact* c = m.contacts + i;

				c->position = out[i].v;
				c->penetration = depths[i];
			}
		}
	}
	else
	{
		n = rotA * n;

		if (dot(n, v) < r32(0.0))
			n = -n;

		vec3 PA, QA;
		vec3 PB, QB;
		q3SupportEdge(rotA, box0.center, eA, n, &PA, &QA);
		q3SupportEdge(rotB, box1.center, eB, -n, &PB, &QB);

		vec3 CA, CB;
		q3EdgesContact(&CA, &CB, PA, QA, PB, QB);

		m.normal = n;
		m.contactCount = 1;

		m.contacts[0].penetration = sMax;
		m.contacts[0].position = (CA + CB) * r32(0.5);
	}
}

TEST(OBB, collision)
{
	Box b0;
	Box b1;

	b0.center = vec3(0, 0, 0);
	b0.halfLength = vec3(1, 1, 1);
	b0.rot = vec4(0, 0, 0, 1);

	b1.center = vec3(0, 1.5, 0);
	b1.halfLength = vec3(1, 1, 1);
	b1.rot = vec4(0, 0, 0, 1);

	q3Manifold manifold;
	q3BoxtoBox(manifold, b0, b1);
	EXPECT_EQ(manifold.contactCount == 4, true);
	EXPECT_EQ(std::abs(manifold.contacts[0].penetration + 0.5f) < REAL_EPSILON, true);
	EXPECT_EQ(std::abs(manifold.contacts[1].penetration + 0.5f) < REAL_EPSILON, true);
	EXPECT_EQ(std::abs(manifold.contacts[2].penetration + 0.5f) < REAL_EPSILON, true);
	EXPECT_EQ(std::abs(manifold.contacts[3].penetration + 0.5f) < REAL_EPSILON, true);

	b1.center = vec3(1.5, 0, 0);
	b1.halfLength = vec3(1, 1, 1);
	b1.rot = vec4(0, 0, 0, 1);

	q3BoxtoBox(manifold, b0, b1);
	EXPECT_EQ(manifold.contactCount == 4, true);
	EXPECT_EQ(std::abs(manifold.contacts[0].penetration + 0.5f) < REAL_EPSILON, true);
	EXPECT_EQ(std::abs(manifold.contacts[1].penetration + 0.5f) < REAL_EPSILON, true);
	EXPECT_EQ(std::abs(manifold.contacts[2].penetration + 0.5f) < REAL_EPSILON, true);
	EXPECT_EQ(std::abs(manifold.contacts[3].penetration + 0.5f) < REAL_EPSILON, true);

	b1.center = vec3(0, 0, 1.5);
	b1.halfLength = vec3(1, 1, 1);
	b1.rot = vec4(0, 0, 0, 1);

	q3BoxtoBox(manifold, b0, b1);
	EXPECT_EQ(manifold.contactCount == 4, true);
	EXPECT_EQ(std::abs(manifold.contacts[0].penetration + 0.5f) < REAL_EPSILON, true);
	EXPECT_EQ(std::abs(manifold.contacts[1].penetration + 0.5f) < REAL_EPSILON, true);
	EXPECT_EQ(std::abs(manifold.contacts[2].penetration + 0.5f) < REAL_EPSILON, true);
	EXPECT_EQ(std::abs(manifold.contacts[3].penetration + 0.5f) < REAL_EPSILON, true);

	Quat1f quat = Quat1f(0.2f, Vec3f(0.2, 0.5, 1));
	b1.center = vec3(0, 1.5, 0);
	b1.halfLength = vec3(1, 1, 1);
	b1.rot = vec4(quat.x, quat.y, quat.z, quat.w);
	q3BoxtoBox(manifold, b0, b1);
	EXPECT_EQ(manifold.contactCount == 1, true);
	EXPECT_EQ(std::abs(manifold.contacts[0].penetration + 0.731658161f) < REAL_EPSILON, true);
}
