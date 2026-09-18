module PathTracerMain;

import Resources;
import Types;
import Random;
import Camera;
import Integrator;

[shader("compute")] [numthreads(16, 16, 1)] void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint imageWidth;
    uint imageHeight;
    outputImage.GetDimensions(imageWidth, imageHeight);
    uint2 pixel = dispatchThreadID.xy;
    if (pixel.x >= imageWidth || pixel.y >= imageHeight) {
        return;
    }
    int2 pixelCoord = int2(pixel);
    int2 imageDimensions = int2(int(imageWidth), int(imageHeight));
    uint pixelIndex = pixel.x + pixel.y * imageWidth;
    Camera camera = makeCamera(imageDimensions);
    float3 colorSum = float3(0.0f);
    int samplesThisDispatch = max(gRender.uSamplesPerDispatch, 1);
    for (int sampleIndex = 0; sampleIndex < samplesThisDispatch; ++sampleIndex) {
        uint absoluteSampleIndex = gRender.uAccumulatedSamples + uint(sampleIndex);
        uint rngState = pcgHash(pcgHash(pixelIndex + 1u) ^ pcgHash(absoluteSampleIndex + 1u));
        Ray ray = getCameraRay(camera, pixelCoord, rngState);
        colorSum += rayColor(ray, rngState);
    }
    float previousSampleCount = float(gRender.uAccumulatedSamples);
    float newSampleCount = float(samplesThisDispatch);
    float3 previousAverage = float3(0.0f);
    if (gRender.uAccumulatedSamples > 0u) {
        previousAverage = outputImage[pixelCoord].rgb;
    }
    float3 accumulatedAverageColor = (previousAverage * previousSampleCount + colorSum) / (previousSampleCount + newSampleCount);
    outputImage[pixelCoord] = float4(accumulatedAverageColor, 1.0f);
}
