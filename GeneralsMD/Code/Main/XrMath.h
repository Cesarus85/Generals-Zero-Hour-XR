// GeneralsX @bugfix Codex 13/09/2026 Shared, host-testable XR transforms.
#pragma once
#include <openxr/openxr.h>
#include <cmath>
#include <cstring>

// FOV bounds are tangents, NOT distances on the near plane. Multiplying
// the x/y scale by nearZ shrinks the image and disagrees with the compositor.
inline void matPerspectiveFromFov(float *m, const XrFovf &fov, float n, float f)
{
	const float l = tanf(fov.angleLeft);
	const float r = tanf(fov.angleRight);
	const float u = tanf(fov.angleUp);
	const float d = tanf(fov.angleDown);
	memset(m, 0, 16 * sizeof(float));
	m[0] = 2.0f / (r - l);
	m[8] = (r + l) / (r - l);
	m[5] = 2.0f / (u - d);
	m[9] = (u + d) / (u - d);
	m[10] = -(f + n) / (f - n);
	m[11] = -1.0f;
	m[14] = -2.0f * f * n / (f - n);
}

// Rigid inverse of a pose -> view matrix (rotation transposed, -R^T * t).
inline void matViewFromPose(float *m, const XrPosef &p)
{
	const float x = p.orientation.x, y = p.orientation.y;
	const float z = p.orientation.z, w = p.orientation.w;
	const float xx = x * x, yy = y * y, zz = z * z;
	const float xy = x * y, xz = x * z, yz = y * z;
	const float wx = w * x, wy = w * y, wz = w * z;
	// Rotation rows (world-from-view), then transposed into m.
	const float r00 = 1.0f - 2.0f * (yy + zz);
	const float r01 = 2.0f * (xy - wz);
	const float r02 = 2.0f * (xz + wy);
	const float r10 = 2.0f * (xy + wz);
	const float r11 = 1.0f - 2.0f * (xx + zz);
	const float r12 = 2.0f * (yz - wx);
	const float r20 = 2.0f * (xz - wy);
	const float r21 = 2.0f * (yz + wx);
	const float r22 = 1.0f - 2.0f * (xx + yy);
	m[0] = r00; m[4] = r10; m[8] = r20; m[12] = 0.0f;
	m[1] = r01; m[5] = r11; m[9] = r21; m[13] = 0.0f;
	m[2] = r02; m[6] = r12; m[10] = r22; m[14] = 0.0f;
	m[3] = 0.0f; m[7] = 0.0f; m[11] = 0.0f; m[15] = 1.0f;
	// t' = -R^T * t.
	const float tx = p.position.x, ty = p.position.y, tz = p.position.z;
	m[12] = -(m[0] * tx + m[4] * ty + m[8] * tz);
	m[13] = -(m[1] * tx + m[5] * ty + m[9] * tz);
	m[14] = -(m[2] * tx + m[6] * ty + m[10] * tz);
}

inline void matMultiply(float *out, const float *a, const float *b)
{
	float tmp[16];
	for (int c = 0; c < 4; c++) {
		for (int r = 0; r < 4; r++) {
			tmp[c * 4 + r] = a[r] * b[c * 4] + a[4 + r] * b[c * 4 + 1] +
			                 a[8 + r] * b[c * 4 + 2] + a[12 + r] * b[c * 4 + 3];
		}
	}
	memcpy(out, tmp, sizeof(tmp));
}

inline void matTranslate(float *m, float x, float y, float z)
{
	memset(m, 0, 16 * sizeof(float));
	m[0] = m[5] = m[10] = m[15] = 1.0f;
	m[12] = x; m[13] = y; m[14] = z;
}

inline void matScale(float *m, float x, float y, float z)
{
	memset(m, 0, 16 * sizeof(float));
	m[0] = x; m[5] = y; m[10] = z; m[15] = 1.0f;
}

// Rotation about X (column-major). Negative angles tip the top away from
// a viewer at +Z -- the tabletop panel's reading tilt.
inline void matRotX(float *m, float radians)
{
	const float c = cosf(radians), s = sinf(radians);
	memset(m, 0, 16 * sizeof(float));
	m[0] = 1.0f;
	m[5] = c; m[9] = -s;
	m[6] = s; m[10] = c;
	m[15] = 1.0f;
}

// Rotation about Y (column-major).
inline void matRotY(float *m, float radians)
{
	const float c = cosf(radians), s = sinf(radians);
	memset(m, 0, 16 * sizeof(float));
	m[0] = c; m[8] = s;
	m[5] = 1.0f;
	m[2] = -s; m[10] = c;
	m[15] = 1.0f;
}

// Yaw-only forward (unit XZ vector) of a head/eye orientation: forward is
// R * (0,0,-1), i.e. minus column 2 of the world-from-view rotation -- the
// same quaternion convention as matViewFromPose. Falls back to LOCAL -Z
// when looking straight up/down (yaw undefined there).
inline void yawForwardFromQuat(const XrQuaternionf &q, float *fx, float *fz)
{
	const float x = q.x, y = q.y, z = q.z, w = q.w;
	const float r02 = 2.0f * (x * z + w * y);
	const float r22 = 1.0f - 2.0f * (x * x + y * y);
	float ux = -r02, uz = -r22;
	const float len = sqrtf(ux * ux + uz * uz);
	if (len < 1e-4f) {
		*fx = 0.0f;
		*fz = -1.0f;
		return;
	}
	*fx = ux / len;
	*fz = uz / len;
}

// Intersect controller -Z with the same scaled unit-width panel that is drawn.
// Return GL-native UV (bottom origin); the engine consumes 1-v as screen Y.
inline bool panelRayUV(const float *model, float aspect, const XrPosef &aim,
                       float *u, float *v)
{
	const auto &q = aim.orientation;
	const float origin[] = {aim.position.x - model[12], aim.position.y - model[13],
	                        aim.position.z - model[14]};
	const float direction[] = {-2.0f * (q.x*q.z + q.w*q.y),
	                           -2.0f * (q.y*q.z - q.w*q.x),
	                           -(1.0f - 2.0f * (q.x*q.x + q.y*q.y))};
	float o[3], d[3];
	for (int c = 0; c < 3; ++c) {
		const float *axis = model + 4*c;
		const float length2 = axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2];
		if (length2 < 1e-8f) return false;
		o[c] = (axis[0]*origin[0] + axis[1]*origin[1] + axis[2]*origin[2]) / length2;
		d[c] = (axis[0]*direction[0] + axis[1]*direction[1] + axis[2]*direction[2]) / length2;
	}
	if (d[2] >= -1e-5f || aspect <= 0.0f) return false;
	const float t = -o[2] / d[2];
	if (t < 0.0f || t > 10.0f) return false;
	*u = o[0] + t*d[0] + 0.5f;
	*v = (o[1] + t*d[1]) / aspect + 0.5f;
	return *u >= 0.0f && *u <= 1.0f && *v >= 0.0f && *v <= 1.0f;
}
