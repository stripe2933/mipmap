# Mipmap

Vulkan mipmap generation with three strategies: blit chain, compute with per-level barriers, and compute with subgroup shuffle.

![Full level mipmap](assets/result.png)

## Usage Guide

> [!TIP]
> This project uses GitHub Action to ensure it can be properly built in Windows (MSVC) and macOS (Clang).
> If you encounter any build issues, refer to the [workflow files](.github/workflows) to see how it works.

The project reads the power-of-2 dimension image, generates full mipmaps by three different strategies, and save the results to an output directory. You can compare the execution times using GPU timestamp query. All core code is in `main.cpp`, and shader code is in the `shaders` directory. Each `subgruop_mipmap_<subgroup-size>.comp` shader file corresponds to an available subgroup size, and the application will choose the appropriate shader file based on the system's subgroup size.

### Build

For compilation, you need:
- C++23 conformant compiler
- CMake ≥ 3.24
- External dependencies
  - [vku](https://github.com/stripe2933/vku)
    - [glslc](https://github.com/google/shaderc/tree/main/glslc): included in Vulkan SDK.
    - [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) and its [C++ binding](https://github.com/YaaZ/VulkanMemoryAllocator-Hpp)
  - [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h)

```bash
mkdir build
cmake -S . -B build -G Ninja                                          \
  -DCMAKE_BUILD_TYPE=Release                                          \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake # Or other package manager what you use
cmake --build build -t mipmap --config Release
```

### Run

For execution, your Vulkan driver must support:
- Vulkan 1.2
- Must support graphics queue.
- Timestamp query: `timestampPeriod` > 0 and `timestampComputeAndGraphics`.
- Subgroup: subgroup size must be at least 8 and must support shuffle operation.
- Device features:
  - `hostQueryReset` (`VK_EXT_host_query_reset`)
  - `storageImageUpdateAfterBind` (`VK_EXT_descriptor_indexing`)
  - `runtimeDescriptorArray` (`VK_EXT_descriptor_indexing`)

If all requirements are satisfied, you can run the executable as:

```bash
./mipmap <image-path> <output-dir>
```

The input image dimensions must be a power of 2, with a minimum size of `32x32`.

In the output directory, three files (`blit.png`, `compute_per_level_barriers.png`, `compute_subgroup.png`) will be generated. Each file corresponds to its respective generation method.

## How does it work?

### Blit chain

Already explained in [vulkan-tutorial](https://vulkan-tutorial.com/Generating_Mipmaps). It blits from level `n-1` to `n`
with image layout transition for every level.

`main.cpp`
```c++
for (auto [srcLevel, dstLevel] : std::views::iota(0U, image.mipLevels) | std::views::pairwise) {
    if (srcLevel != 0U){
        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
            {}, {}, {},
            std::array {
                vk::ImageMemoryBarrier {
                    vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead,
                    vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
                    {}, {},
                    image,
                    { vk::ImageAspectFlagBits::eColor, srcLevel, 1, 0, 1 }
                },
                vk::ImageMemoryBarrier {
                    {}, vk::AccessFlagBits::eTransferWrite,
                    {}, vk::ImageLayout::eTransferDstOptimal,
                    {}, {},
                    image,
                    { vk::ImageAspectFlagBits::eColor, dstLevel, 1, 0, 1 }
                },
            });
    }

    commandBuffer.blitImage(
        image, vk::ImageLayout::eTransferSrcOptimal,
        image, vk::ImageLayout::eTransferDstOptimal,
        vk::ImageBlit {
            { vk::ImageAspectFlagBits::eColor, srcLevel, 0, 1 },
            { vk::Offset3D{}, vk::Offset3D { vku::convertOffset2D(image.mipExtent(srcLevel)), 1 } },
            { vk::ImageAspectFlagBits::eColor, dstLevel, 0, 1 },
            { vk::Offset3D{}, vk::Offset3D { vku::convertOffset2D(image.mipExtent(dstLevel)), 1 } },
        },
        vk::Filter::eLinear);
}
```

| Pros ✅ | Cons ❌                                                                                                                                                                                     |
|--------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Simple to implement | - Requires multiple image layout transitions<br/>- Requires a graphics-capable queue (not a serious problem, but could be inconvenient if dealing with compute-specialized queue families) |

### Compute with per-level barriers

It uses `16x16` workgroup size, and each invocation fetches 4 texels from the source image.

`mipmap.comp`
```glsl
void main(){
    vec4 averageColor
        = imageLoad(mipImages[srcLevel], 2 * ivec2(gl_GlobalInvocationID.xy))
        + imageLoad(mipImages[srcLevel], 2 * ivec2(gl_GlobalInvocationID.xy) + ivec2(1, 0))
        + imageLoad(mipImages[srcLevel], 2 * ivec2(gl_GlobalInvocationID.xy) + ivec2(0, 1))
        + imageLoad(mipImages[srcLevel], 2 * ivec2(gl_GlobalInvocationID.xy) + ivec2(1, 1));
    averageColor /= 4.0;
    imageStore(mipImages[dstLevel], ivec2(gl_GlobalInvocationID.xy), averageColor);
}
```

`main.cpp`
```c++
commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, descriptorSets, {});
for (auto [srcLevel, dstLevel] : std::views::iota(0U, mipLevels) | std::views::pairwise) {
    if (srcLevel != 0U) {
        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
            {}, 
            vk::MemoryBarrier {
                vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead,
            }, 
            {}, {});
    }

    commandBuffer.pushConstants<PushConstant>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, PushConstant { srcLevel });
    commandBuffer.dispatch(
        divCeil(baseImageExtent.width >> dstLevel, 16U),
        divCeil(baseImageExtent.height >> dstLevel, 16U),
        1);
}
```

This method has some advantages and disadvantages compared to the blit chain:

| Pros ✅                                                             | Cons ❌                                                                                                                                                                                                                                                                                       |
|--------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| - Compute only<br/>- No image layout transition between dispatches | - Still requires memory barrier between dispatches<br/>- Managing pipelines and descriptor sets is challenging<br/>- Slower than the blit chain (due to the need to compile the shader and bind the pipeline and descriptors)<br/>- Cannot be used for images with non-power of 2 dimensions |

### Compute with subgroup shuffle

This is the main concept, so I'll explain it in detail.

#### What is subgroup?

In shader, invocations within a specific group can communicate without explicit synchronization, a concept known as a ***subgroup***. The size of subgroups varies by system, for example, subgroups on NVIDIA GPUs are typically 32, while on AMD GPUs, they are usually 64.

**If a subgroup holds 2x2 texel data, it can average them into 1 texel without explicit synchronization**. This can be extended to `2^n x 2^n` texel data, which can then be mipmapped into 1 texel, provided the subgroup size is sufficient.

#### Mapping from invocation ID to texel fetch position

Assuming the workgroup size is `16x16` and the subgroup size is `32`, a total of `256 / 32 = 8` subgroups are arranged linearly within the workgroup.

![8 subgroups laid in `16x16` workgroup](assets/subgroup_linear.png)

Each `16x2` subgroup is filled with alternating background colors for distinction. The numbers in each cell represent `gl_SubgroupInvocationID`.

It would be suboptimal for an invocation within a subgroup to fetch the texel at `gl_GlobalInvocationID.xy` as before because they can only average `2x2` texels. For example, in the first subgroup in the figure (filled with light blue), IDs 0, 1, 16 and 17 can communicate and average `2x2` region into 1 pixel. However, they cannot average a `4x4` region into 1 pixel because the row count of a subgroup is 2. To maximize efficiency, **the texel fetching pattern within a subgroup needs to be as square as possible**.

![Texel fetch position for every invocations](assets/sample-mapping-linear.png)

In the figure, the optimal texel fetching position for each invocation is shown. With this mapping, the subgroups can average a `4x4` region into 1 pixel.

![Texel fetch position aligned by sample position](assets/sample-mapping-aligned.png)

If we plot `gl_LocalInvocationID.xy` with their respective texel fetching positions, the figure would look like the one above. Each subgroup is filled with distinct colors. In this case, the subgroup extent is `8x4`, which is more square-shaped than before.

![subgroup_square.png](assets/subgroup_square.png)

Now, each subgroup can average two `4x4` regions (`0..3, 8..11, 16..19, 24..27` and `4..7, 12..15, 20..23, 28..31`) to 1 pixel.

We need a mapping between texel fetch location and `gl_LocalInvocationID`. It would be

```glslc
ivec2 sampleLocation = gl_WorkGroupSize.xy * gl_WorkGroupID.xy + ivec2(
    gl_LocalInvocationID.x % 8 + (gl_LocalInvocationID.y / 8) * 8,
    ((gl_LocalInvocationID.y / 2) * 2 + gl_LocalInvocationID.x / 8) % 16
);
```

Or a more bit manipulation-friendly version:

```glslc
ivec2 samplePosition = gl_WorkgroupSize.xy * gl_WorkGroupID.xy + ivec2(
    (gl_LocalInvocationID.x & 7U) | (gl_LocalInvocationID.y & ~7U),
    ((gl_LocalInvocationID.y << 1U) | (gl_LocalInvocationID.x >> 3U)) & 15U
);
```

#### Subgroup shuffle

With properly mapped texels in the subgroup, we can use ***Subgroup shuffle*** to average the invocation quads.

![Subgroup ID in binary form](assets/subgroup-id-binary.png)

The figure above visualizes `gl_SubgroupInvocationID` in binary form within a subgroup. Using [`subgroupShuffleXor`](https://www.khronos.org/blog/vulkan-subgroup-tutorial), an invocation (let's denote its `gl_SubgroupInvocationID` as `i`) can access the data of the invocation whose `gl_SubgroupInvocationID` is `i ^ <constant-value>`, i.e. bitwise XOR.

For example, the following shader for the invocations whose `gl_SubgroupID`s are `0, 1, 8, 9` will execute as:

```glsl
vec4 averageColor = imageLoad(baseImage, samplePosition);

// | gl_SubgroupInvocationID |  gl_LocalInvocationID.xy  |   averageColor    |
// |-------------------------|---------------------------|-------------------|
// |       0(=0b00000)       |           (0, 0)          |  baseImage[0, 0]  |--┐
// |-------------------------|---------------------------|-------------------|  |
// |       1(=0b00001)       |           (1, 0)          |  baseImage[1, 0]  |--|--┐
// |-------------------------|---------------------------|-------------------|  |  |
// |       8(=0b01000)       |           (0, 1)          |  baseImage[0, 1]  |--|--|--┐
// |-------------------------|---------------------------|-------------------|  |  |  |
// |       9(=0b01001)       |           (1, 1)          |  baseImage[1, 1]  |--|--|--|--┐
                                                                            //  |  |  |  |
averageColor += subgroupShuffleXor(averageColor, 1);                        //  |  |  |  |
                                                                            //  |  |  |  |
// | gl_SubgroupInvocationID | gl_SubgroupInvocationID^1 |   averageColor    |  |  |  |  |
// |-------------------------|---------------------------|-------------------|  |  |  |  |
// |       0(=0b00000)       |         1(=0b00001)       | baseImage[0, 0]   |  |  |  |  |
// |                         |                           | + baseImage[1, 0] |<-|--┘  |  |  --┐
// |-------------------------|---------------------------|-------------------|  |     |  |    |
// |       1(=0b00001)       |         0(=0b00000)       | baseImage[1, 0]   |  |     |  |    |
// |                         |                           | + baseImage[0, 0] |<-┘     |  |  --|--┐
// |-------------------------|---------------------------|-------------------|        |  |    |  |
// |       8(=0b01000)       |         9(=0b01001)       | baseImage[0, 1]   |        |  |    |  |
// |                         |                           | + baseImage[1, 1] |<-------|--┘  --|--|--┐
// |-------------------------|---------------------------|-------------------|        |       |  |  |
// |       9(=0b01001)       |         8(=0b01000)       | baseImage[1, 1]   |        |       |  |  |
// |                         |                           | + baseImage[0, 1] |<-------┘     --|--|--|--┐
                                                                            //                |  |  |  |
averageColor += subgroupShuffleXor(averageColor, 8);                        //                |  |  |  |
                                                                            //                |  |  |  |
// | gl_SubgroupInvocationID | gl_SubgroupInvocationID^8 |   averageColor    |                |  |  |  |
// |-------------------------|---------------------------|-------------------|                |  |  |  |
// |       0(=0b00000)       |         8(=0b01000)       | baseImage[0, 0]   |                |  |  |  |
// |                         |                           | + baseImage[1, 0] |                |  |  |  |
// |                         |                           | + baseImage[0, 1] |<---------------|--|--┘  |
// |                         |                           | + baseImage[1, 1] |                |  |     |
// |-------------------------|---------------------------|-------------------|                |  |     |
// |       1(=0b00001)       |         9(=0b01001)       | baseImage[1, 0]   |                |  |     |
// |                         |                           | + baseImage[1, 1] |                |  |     |
// |                         |                           | + baseImage[0, 0] |<---------------|--|-----┘
// |                         |                           | + baseImage[0, 1] |                |  |
// |-------------------------|---------------------------|-------------------|                |  |
// |       8(=0b01000)       |         0(=0b00000)       | baseImage[0, 1]   |                |  |
// |                         |                           | + baseImage[1, 1] |                |  |
// |                         |                           | + baseImage[0, 0] |<---------------┘  |
// |                         |                           | + baseImage[1, 0] |                   |
// |-------------------------|---------------------------|-------------------|                   |
// |       9(=0b01001)       |         1(=0b00001)       | baseImage[1, 1]   |                   |
// |                         |                           | + baseImage[0, 1] |                   |
// |                         |                           | + baseImage[1, 0] |<------------------┘
// |                         |                           | + baseImage[0, 0] |

averageColor /= 4.0;
```

Therefore, `averageColor += subgroupShuffleXor(averageColor, 1)` sums the horizontally adjacent `averageColor`, and consequent `averageColor += subgroupShuffleXor(averageColor, 8)` sums the vertically adjacent `averageColor`, which sums up the `averageColor` of 2x2 invocations into one.

After the averaging, the next `subgroupShuffleXor` would be:

```glsl
averageColor += subgroupShuffleXor(averageColor, 2);
averageColor += subgroupShuffleXor(averageColor, 16);
averageColor /= 4.0;
```

Because all adjacent quads have the same averageColor in the previous phase, they must communicate with the next 2-stride adjacent quad.

### Shared memory with barrier

If your system's subgroup size is 32, it can form an `8x4` subgroup extent, allowing two `4x4` regions to be mipmapped into 1 pixel. Even if you're using an AMD GPU with a subgroup size of 64, it can form an `8x8` subgroup extent and an `8x8` region can be mipmapped. However, this is still far from our goal of mipmapping a `32x32` region into 1 pixel. To achieve this, we'll use shared memory.

Each subgroup stores its `averageColor` in shared memory, and after synchronization (`memoryBarrierShared(); barrier();`), the representative invocation in each `2^n x 2^n` region will average the shared `averageColor`. For a subgroup size of 32, this must be executed twice.

`subgroup_mipmap_32.comp`
```glsl
averageColor += subgroupShuffleXor(averageColor, 4U /* 0b00100 */);
if (subgroupElect()){
    sharedData[gl_SubgroupID] = averageColor;
}

memoryBarrierShared();
barrier();

if ((gl_SubgroupID & 1U) == 1U){
    averageColor = (sharedData[gl_SubgroupID] + sharedData[gl_SubgroupID ^ 1U]) / 4.f;
    imageStore(mipImages[pc.baseLevel + 4U], sampleCoordinate >> 3, averageColor);
}

if (gl_LocalInvocationIndex == 0U){
    averageColor = (sharedData[0] + sharedData[1] + sharedData[2] + sharedData[3] + sharedData[4] + sharedData[5] + sharedData[6] + sharedData[7]) / 16.f;
    imageStore(mipImages[pc.baseLevel + 5U], sampleCoordinate >> 4, averageColor);
}
```

With a larger subgroup size, fewer synchronization barriers are needed, improving performance.

#### How this can be fast?

This method can be faster than the previous compute-based strategy because it requires significantly fewer memory barriers (10 vs 1 for `1024x1024` image). Each `32x32` region can be mipmapped with just 1 dispatch. For every dispatch, shaders load the texels from the image (stored in VRAM) and store them back into the image repeatedly. With subgroup operation, each level, except the first `imageLoad` phase, allows `averageColor` to be loaded from the L2 cache, which is much faster than loading from VRAM.

#### What is the downside of this method?

Although this method is faster than the previous compute-based strategy, it may still slower than the blit-chain method if you're using a non-compute-specialized queue or just mipmapping a single image. It shares the same downsides as the previous one (needing to manage pipeline and descriptor sets, and being usable only with power of 2 images) and has additional constraint: the image size must be at least `32x32` and your last dispatch input image size must be `32x32`. For example, if you're mipmapping a `256x256` image, the dispatch sequence would be:

1. `PushConstant { .baseLevel = 0, .remainingMipLevels = 3 }`: `256x256` -> `32x32`
2. `PushConstant { .baseLevel = 3, .remainingMipLevels = 5 }`: `32x32` -> `1x1`

Refer to the `SubgroupMipmapComputer::compute` method to see how it works.

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE.txt) file for details.
```