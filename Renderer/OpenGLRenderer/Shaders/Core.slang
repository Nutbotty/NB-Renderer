#language slang 2026
module Core;

import Constants;
import Types;

public Ray makeRay(float3 origin, float3 direction) {
    Ray result;
    result.origin = origin;
    result.direction = direction;
    return result;
}

public Interval makeInterval(float minValue, float maxValue) {
    Interval result;
    result.min = minValue;
    result.max = maxValue;
    return result;
}

public float3 transformPoint(float4x4 transform, float3 point) {
    return mul(float4(point, 1.0f), transform).xyz;
}

public float3 transformVector(float4x4 transform, float3 vectorValue) {
    return mul(float4(vectorValue, 0.0f), transform).xyz;
}

public Ray transformRay(float4x4 transform, Ray ray) {
    Ray transformed;
    transformed.origin = transformPoint(transform, ray.origin);
    transformed.direction = transformVector(transform, ray.direction);
    return transformed;
}

public float3 rayAt(Ray ray, float t) {
    return ray.origin + t * ray.direction;
}

public void setFaceNormal(Ray ray, float3 outwardNormal, inout HitRecord record) {
    record.frontFace = dot(ray.direction, outwardNormal) < 0.0f;
    record.normal = record.frontFace ? outwardNormal : -outwardNormal;
}

public float intervalSize(Interval interval) {
    return interval.max - interval.min;
}

public bool intervalContains(Interval interval, float value) {
    return interval.min <= value && value <= interval.max;
}

public bool intervalSurrounds(Interval interval, float value) {
    return interval.min < value && value < interval.max;
}

public float intervalClamp(Interval interval, float value) {
    return clamp(value, interval.min, interval.max);
}
