#language slang 2026
module Bvh;

import Constants;
import Types;
import GpuTypes;
import Core;
import Resources;
import Geometry;

public bool hitMesh(int meshIndex, Ray localRay, Interval rayT, out HitRecord record) {
    PackedMesh mesh = meshBuffer[meshIndex];
    int rootNode = mesh.metadata.x;
    bool hitAnything = false;
    float closestSoFar = rayT.max;
    HitRecord temporaryRecord;
    float3 inverseDirection = 1.0f / localRay.direction;
    float rootEntryT;
    if (!hitAabb(blasNodeBuffer[rootNode], localRay, inverseDirection, rayT.min, closestSoFar, rootEntryT)) {
        return false;
    }
    int stack[BLAS_STACK_SIZE];
    int stackSize = 0;
    stack[stackSize++] = rootNode;
    while (stackSize > 0) {
        int nodeIndex = stack[--stackSize];
        PackedBvhNode node = blasNodeBuffer[nodeIndex];
        bool isLeaf = node.metadata.z != 0;
        if (isLeaf) {
            int firstTriangle = node.metadata.x;
            int triangleCount = node.metadata.y;
            for (int triangleOffset = 0; triangleOffset < triangleCount; ++triangleOffset) {
                int triangleIndex = firstTriangle + triangleOffset;
                if (hitMeshTri(triangleIndex, localRay, makeInterval(rayT.min, closestSoFar), temporaryRecord)) {
                    hitAnything = true;
                    closestSoFar = temporaryRecord.t;
                    record = temporaryRecord;
                }
            }
            continue;
        }
        int leftChild = node.metadata.x;
        int rightChild = node.metadata.y;
        float leftEntryT;
        float rightEntryT;
        bool hitLeft = hitAabb(blasNodeBuffer[leftChild], localRay, inverseDirection, rayT.min, closestSoFar, leftEntryT);
        bool hitRight = hitAabb(blasNodeBuffer[rightChild], localRay, inverseDirection, rayT.min, closestSoFar, rightEntryT);
        if (hitLeft && hitRight) {
            // Push farther first so the nearer child is popped first.
            if (leftEntryT < rightEntryT) {
                stack[stackSize++] = rightChild;
                stack[stackSize++] = leftChild;
            } else {
                stack[stackSize++] = leftChild;
                stack[stackSize++] = rightChild;
            }
        } else if (hitLeft) {
            stack[stackSize++] = leftChild;
        } else if (hitRight) {
            stack[stackSize++] = rightChild;
        }
    }
    return hitAnything;
}

public bool hitLocalPrimitive(int primitiveType, int primitiveIndex, Ray ray, Interval rayT, out HitRecord record) {
    if (primitiveType == PRIMITIVE_SPHERE) {
        return hitSphere(loadSphere(primitiveIndex), ray, rayT, record);
    }
    if (primitiveType == PRIMITIVE_QUAD) {
        return hitQuad(primitiveIndex, ray, rayT, record);
    }
    if (primitiveType == PRIMITIVE_TRI) {
        return hitTri(primitiveIndex, ray, rayT, record);
    }
    if (primitiveType == PRIMITIVE_MESH) {
        return hitMesh(primitiveIndex, ray, rayT, record);
    }
    return false;
}

public bool hitPrimitive(PackedPrimitiveRef primitiveRef, Ray ray, Interval rayT, out HitRecord record) {
    int primitiveType = primitiveRef.metadata.x;
    int primitiveIndex = primitiveRef.metadata.y;
    int transformIndex = primitiveRef.metadata.z;
    int materialOffset = primitiveRef.metadata.w;
    if (transformIndex == 0) {
        if (!hitLocalPrimitive(primitiveType, primitiveIndex, ray, rayT, record)) {
            return false;
        }
        if (primitiveType == PRIMITIVE_MESH) {
            int materialSlot = record.material;
            if (materialOffset < 0 || materialSlot < 0) {
                record.material = 0;
            } else {
                uint materialCount = uint(instanceMaterialBuffer.getCount());
                int materialIndex = materialOffset + materialSlot;
                if (materialIndex < 0 || uint(materialIndex) >= materialCount) {
                    record.material = 0;
                } else {
                    record.material = instanceMaterialBuffer[materialIndex];
                }
            }
        }
        return true;
    }
    float4x4 worldToObject = transformBuffer[transformIndex].worldToObject;
    Ray localRay = transformRay(worldToObject, ray);
    HitRecord localRecord;
    if (!hitLocalPrimitive(primitiveType, primitiveIndex, localRay, rayT, localRecord)) {
        return false;
    }
    record = localRecord;

    // localRay.direction intentionally remains unnormalized so localRecord.t
    // stays compatible with the original world-space ray.
    record.p = rayAt(ray, localRecord.t);
    float3 localOutwardNormal = localRecord.frontFace ? localRecord.normal : -localRecord.normal;
    float3x3 normalToWorld = transpose((float3x3)worldToObject);
    float3 worldOutwardNormal = normalize(mul(localOutwardNormal, normalToWorld));
    setFaceNormal(ray, worldOutwardNormal, record);
    if (primitiveType == PRIMITIVE_MESH) {
        int materialSlot = record.material;
        int materialIndex = materialOffset + materialSlot;
        uint materialCount = uint(instanceMaterialBuffer.getCount());
        if (materialOffset < 0 || materialSlot < 0 || materialIndex < 0 || uint(materialIndex) >= materialCount) {
            record.material = 0;
        } else {
            record.material = instanceMaterialBuffer[materialIndex];
        }
    }
    return true;
}

public bool hitWorld(Ray ray, Interval rayT, out HitRecord record) {
    if (gRender.uTlasNodeCount <= 0) {
        return false;
    }
    bool hitAnything = false;
    float closestSoFar = rayT.max;
    HitRecord temporaryRecord;
    float rootEntryT;
    float3 inverseDirection = 1.0f / ray.direction;
    if (!hitAabb(tlasNodeBuffer[0], ray, inverseDirection, rayT.min, closestSoFar, rootEntryT)) {
        return false;
    }
    int stack[TLAS_STACK_SIZE];
    int stackSize = 0;
    stack[stackSize++] = 0;
    while (stackSize > 0) {
        int nodeIndex = stack[--stackSize];
        PackedBvhNode node = tlasNodeBuffer[nodeIndex];
        bool isLeaf = node.metadata.z != 0;
        if (isLeaf) {
            int firstPrimitive = node.metadata.x;
            int primitiveCount = node.metadata.y;
            for (int primitiveOffset = 0; primitiveOffset < primitiveCount; ++primitiveOffset) {
                int primitiveRefIndex = firstPrimitive + primitiveOffset;
                PackedPrimitiveRef primitiveRef = primitiveRefBuffer[primitiveRefIndex];
                if (hitPrimitive(primitiveRef, ray, makeInterval(rayT.min, closestSoFar), temporaryRecord)) {
                    hitAnything = true;
                    closestSoFar = temporaryRecord.t;
                    record = temporaryRecord;
                }
            }
            continue;
        }
        int leftChild = node.metadata.x;
        int rightChild = node.metadata.y;
        PackedBvhNode leftNode = tlasNodeBuffer[leftChild];
        PackedBvhNode rightNode = tlasNodeBuffer[rightChild];
        float leftEntryT;
        float rightEntryT;
        bool hitLeft = hitAabb(leftNode, ray, inverseDirection, rayT.min, closestSoFar, leftEntryT);
        bool hitRight = hitAabb(rightNode, ray, inverseDirection, rayT.min, closestSoFar, rightEntryT);
        if (hitLeft && hitRight) {
            if (leftEntryT < rightEntryT) {
                stack[stackSize++] = rightChild;
                stack[stackSize++] = leftChild;
            } else {
                stack[stackSize++] = leftChild;
                stack[stackSize++] = rightChild;
            }
        } else if (hitLeft) {
            stack[stackSize++] = leftChild;
        } else if (hitRight) {
            stack[stackSize++] = rightChild;
        }
    }
    return hitAnything;
}
