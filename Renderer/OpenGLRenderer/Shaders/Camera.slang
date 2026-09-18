#language slang 2026
module Camera;

import Types;
import Core;
import Random;
import Resources;

public Camera makeCamera(int2 imageDimensions) {
    Camera camera;
    camera.center = gRender.uCameraLookFrom;
    float imageWidth = float(imageDimensions.x);
    float imageHeight = float(imageDimensions.y);
    camera.aspectRatio = imageWidth / imageHeight;

    // w points backward from target toward camera.
    float3 w = normalize(gRender.uCameraLookFrom - gRender.uCameraLookAt);
    float3 u = normalize(cross(gRender.uCameraVUp, w));
    float3 v = cross(w, u);
    float theta = radians(gRender.uCameraVerticalFov);
    float viewportHeight = 2.0f * tan(theta * 0.5f) * gRender.uCameraFocusDistance;
    float viewportWidth = viewportHeight * camera.aspectRatio;
    float3 viewportU = viewportWidth * u;
    float3 viewportV = viewportHeight * v;
    camera.pixel_delta_u = viewportU / imageWidth;
    camera.pixel_delta_v = viewportV / imageHeight;
    float3 viewportUpperLeft = camera.center - gRender.uCameraFocusDistance * w - viewportU * 0.5f - viewportV * 0.5f;
    camera.pixel00_loc = viewportUpperLeft + 0.5f * (camera.pixel_delta_u + camera.pixel_delta_v);
    float defocusRadius = gRender.uCameraFocusDistance * tan(radians(gRender.uCameraDefocusAngle * 0.5f));
    camera.defocus_disc_u = u * defocusRadius;
    camera.defocus_disc_v = v * defocusRadius;
    return camera;
}

public float3 defocusDiskSample(Camera camera, inout uint rngState) {
    float2 p = randomInUnitDisk(rngState);
    return camera.center + p.x * camera.defocus_disc_u + p.y * camera.defocus_disc_v;
}

public Ray getCameraRay(Camera camera, int2 pixel, inout uint rngState) {
    float2 offset = sampleSquare(rngState);
    float3 pixelSample = camera.pixel00_loc + (float(pixel.x) + offset.x) * camera.pixel_delta_u + (float(pixel.y) + offset.y) * camera.pixel_delta_v;
    float3 rayOrigin;
    if (gRender.uCameraDefocusAngle <= 0.0f) {
        rayOrigin = camera.center;
    } else {
        rayOrigin = defocusDiskSample(camera, rngState);
    }
    return makeRay(rayOrigin, pixelSample - rayOrigin);
}
