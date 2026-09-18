#language slang 2026
module Geometry;

import Constants;
import Types;
import GpuTypes;
import Core;
import Resources;

public Sphere loadSphere(int index) {
    PackedSphere gpuSphere = sphereBuffer[index];
    Sphere sphere;
    sphere.center = gpuSphere.centerRadius.xyz;
    sphere.radius = gpuSphere.centerRadius.w;
    sphere.material = gpuSphere.metadata.x;
    return sphere;
}

public bool hitAabb(PackedBvhNode node, Ray ray, float3 inverseDirection, float rayTMin, float rayTMax, out float entryT) {
    float3 t0 = (node.boundsMin.xyz - ray.origin) * inverseDirection;
    float3 t1 = (node.boundsMax.xyz - ray.origin) * inverseDirection;
    float3 nearT = min(t0, t1);
    float3 farT = max(t0, t1);
    float currentMin = max(rayTMin, max(nearT.x, max(nearT.y, nearT.z)));
    float currentMax = min(rayTMax, min(farT.x, min(farT.y, farT.z)));
    entryT = currentMin;
    return currentMax >= currentMin;
}

public bool hitSphere(Sphere sphere, Ray ray, Interval rayT, out HitRecord record) {
    float3 oc = ray.origin - sphere.center;
    float a = dot(ray.direction, ray.direction);
    float h = dot(oc, ray.direction);
    float c = dot(oc, oc) - sphere.radius * sphere.radius;
    float discriminant = h * h - a * c;
    if (discriminant < 0.0f) {
        return false;
    }
    float sqrtDiscriminant = sqrt(discriminant);
    float root = (-h - sqrtDiscriminant) / a;
    if (!intervalSurrounds(rayT, root)) {
        root = (-h + sqrtDiscriminant) / a;
        if (!intervalSurrounds(rayT, root)) {
            return false;
        }
    }
    record.t = root;
    record.p = rayAt(ray, record.t);
    float3 outwardNormal = (record.p - sphere.center) / sphere.radius;
    setFaceNormal(ray, outwardNormal, record);
    record.material = sphere.material;
    record.uv = float2(0.0f);
    return true;
}

public bool hitQuad(int quadIndex, Ray ray, Interval rayT, out HitRecord record) {
    PackedQuad quad = quadBuffer[quadIndex];
    float3 Q = quad.Q.xyz;
    float3 u = quad.u.xyz;
    float3 v = quad.v.xyz;
    float3 n = cross(u, v);
    float nLengthSquared = dot(n, n);
    if (nLengthSquared < EPSILON) {
        return false;
    }
    float3 normal = n / sqrt(nLengthSquared);
    float denom = dot(normal, ray.direction);
    if (abs(denom) < EPSILON) {
        return false;
    }
    float D = dot(normal, Q);
    float t = (D - dot(normal, ray.origin)) / denom;
    if (!intervalSurrounds(rayT, t)) {
        return false;
    }
    float3 intersection = rayAt(ray, t);
    float3 planarHit = intersection - Q;
    float3 w = n / nLengthSquared;
    float alpha = dot(w, cross(planarHit, v));
    float beta = dot(w, cross(u, planarHit));
    if (alpha < 0.0f || alpha > 1.0f || beta < 0.0f || beta > 1.0f) {
        return false;
    }
    record.t = t;
    record.p = intersection;
    record.material = quad.metadata.x;
    record.uv = float2(alpha, beta);
    setFaceNormal(ray, normal, record);
    return true;
}

public bool hitTri(int triIndex, Ray ray, Interval rayT, out HitRecord record) {
    PackedTriangle tri = triangleBuffer[triIndex];
    float3 Q = tri.Q.xyz;
    float3 U = tri.U.xyz;
    float3 V = tri.V.xyz;
    float3 edge1 = U - Q;
    float3 edge2 = V - Q;
    float3 normal = normalize(cross(edge1, edge2));
    float3 h = cross(ray.direction, edge2);
    float determinant = dot(edge1, h);
    if (abs(determinant) < EPSILON) {
        return false;
    }
    float inverseDeterminant = 1.0f / determinant;
    float3 s = ray.origin - Q;
    float u = inverseDeterminant * dot(s, h);
    if (u < EPSILON || u - 1.0f > EPSILON) {
        return false;
    }
    float3 q = cross(s, edge1);
    float v = inverseDeterminant * dot(ray.direction, q);
    if (v < -EPSILON || u + v - 1.0f > EPSILON) {
        return false;
    }
    float t = inverseDeterminant * dot(edge2, q);
    if (!intervalSurrounds(rayT, t)) {
        return false;
    }
    record.t = t;
    record.p = rayAt(ray, t);
    record.material = tri.metadata.x;
    record.uv = float2(u, v);
    setFaceNormal(ray, normal, record);
    return true;
}

public bool hitMeshTri(int triIndex, Ray ray, Interval rayT, out HitRecord record) {
    PackedMeshTriangle tri = meshTriangleBuffer[triIndex];
    int index0 = tri.metadata.x;
    int index1 = tri.metadata.y;
    int index2 = tri.metadata.z;
    float3 a = meshVertexBuffer[index0].position.xyz;
    float3 b = meshVertexBuffer[index1].position.xyz;
    float3 c = meshVertexBuffer[index2].position.xyz;
    float3 edge1 = b - a;
    float3 edge2 = c - a;
    float3 p = cross(ray.direction, edge2);
    float determinant = dot(edge1, p);
    if (abs(determinant) < EPSILON) {
        return false;
    }
    float inverseDeterminant = 1.0f / determinant;
    float3 s = ray.origin - a;
    float u = inverseDeterminant * dot(s, p);
    if (u < 0.0f || u > 1.0f) {
        return false;
    }
    float3 q = cross(s, edge1);
    float v = inverseDeterminant * dot(ray.direction, q);
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }
    float t = inverseDeterminant * dot(edge2, q);
    if (!intervalSurrounds(rayT, t)) {
        return false;
    }
    record.t = t;
    record.p = rayAt(ray, t);

    // Material is still an object-local slot here.
    record.material = tri.metadata.w;
    float w = 1.0f - u - v;
    float3 n0 = meshVertexBuffer[index0].normal.xyz;
    float3 n1 = meshVertexBuffer[index1].normal.xyz;
    float3 n2 = meshVertexBuffer[index2].normal.xyz;
    float3 interpolatedNormal = w * n0 + u * n1 + v * n2;
    float3 outwardNormal;
    if (dot(interpolatedNormal, interpolatedNormal) > EPSILON) {
        outwardNormal = normalize(interpolatedNormal);
    } else {
        outwardNormal = normalize(cross(edge1, edge2));
    }
    setFaceNormal(ray, outwardNormal, record);
    float2 uv0 = meshVertexBuffer[index0].texCoord.xy;
    float2 uv1 = meshVertexBuffer[index1].texCoord.xy;
    float2 uv2 = meshVertexBuffer[index2].texCoord.xy;
    record.uv = w * uv0 + u * uv1 + v * uv2;
    return true;
}
