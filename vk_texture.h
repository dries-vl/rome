#pragma once

#define MAX_TEXTURES 64
#define MAX_DETAIL_TEXTURES 2

extern const unsigned char font_atlas[];
extern const unsigned char font_atlas_end[];
#define font_atlas_len ((size_t)(font_atlas_end - font_atlas))

extern const unsigned char map[];
extern const unsigned char map_end[];
#define map_len ((size_t)(map_end - map))

extern const unsigned char height[];
extern const unsigned char height_end[];
#define height_len ((size_t)(height_end - height))

extern const unsigned char height_detail[];
extern const unsigned char height_detail_end[];
#define height_detail_len ((size_t)(height_detail_end - height_detail))

extern const unsigned char normals[];
extern const unsigned char normals_end[];
#define normals_len ((size_t)(normals_end - normals))

extern const unsigned char noise[];
extern const unsigned char noise_end[];
#define noise_len ((size_t)(noise_end - noise))

typedef struct Texture {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
} Texture;

enum {
    DETAIL_TEXTURE_TERRAIN = 0,
    DETAIL_TEXTURE_HEIGHT = 1
};

static Texture textures[MAX_TEXTURES];
static Texture detail_textures[MAX_DETAIL_TEXTURES];
static Texture dummy_texture;
static VkSampler global_sampler;
static uint32_t texture_count = 0;

static void create_image_2d_mipped(
    VkDevice dev,
    VkPhysicalDevice phys,
    uint32_t w,
    uint32_t h,
    VkFormat format,
    uint32_t mipLevels,
    VkImageUsageFlags usage,
    VkImage* out_img,
    VkDeviceMemory* out_mem
) {
    VkImageCreateInfo ici;
    memset(&ici, 0, sizeof(ici));
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = format;
    ici.extent.width = w;
    ici.extent.height = h;
    ici.extent.depth = 1;
    ici.mipLevels = mipLevels;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK(vkCreateImage(dev, &ici, NULL, out_img));

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(dev, *out_img, &mr);

    VkMemoryAllocateInfo mai;
    memset(&mai, 0, sizeof(mai));
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = find_memory_type_index(
        phys,
        mr.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    VK_CHECK(vkAllocateMemory(dev, &mai, NULL, out_mem));
    VK_CHECK(vkBindImageMemory(dev, *out_img, *out_mem, 0));
}

static void create_view_2d_mipped(
    VkDevice dev,
    VkImage img,
    VkFormat fmt,
    uint32_t mipLevels,
    VkImageView* out_view
) {
    VkImageViewCreateInfo vci;
    memset(&vci, 0, sizeof(vci));
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image = img;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = fmt;
    vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vci.subresourceRange.baseMipLevel = 0;
    vci.subresourceRange.levelCount = mipLevels;
    vci.subresourceRange.baseArrayLayer = 0;
    vci.subresourceRange.layerCount = 1;
    VK_CHECK(vkCreateImageView(dev, &vci, NULL, out_view));
}

static void upload_image_2d(
    VkDevice dev,
    VkPhysicalDevice phys,
    VkQueue q,
    VkCommandPool pool,
    VkImage image,
    uint32_t w,
    uint32_t h,
    const void* pixels,
    VkDeviceSize size
) {
    VkBuffer staging;
    VkDeviceMemory staging_mem;
    create_buffer_and_memory(
        dev,
        phys,
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging,
        &staging_mem
    );
    upload_to_buffer(dev, staging_mem, 0, pixels, size);

    VkCommandBuffer cmd = begin_single_use_cmd(dev, pool);

    VkImageMemoryBarrier2 to_copy;
    memset(&to_copy, 0, sizeof(to_copy));
    to_copy.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    to_copy.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    to_copy.srcAccessMask = 0;
    to_copy.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    to_copy.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    to_copy.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    to_copy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_copy.image = image;
    to_copy.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_copy.subresourceRange.baseMipLevel = 0;
    to_copy.subresourceRange.levelCount = 1;
    to_copy.subresourceRange.baseArrayLayer = 0;
    to_copy.subresourceRange.layerCount = 1;

    VkDependencyInfo dep0;
    memset(&dep0, 0, sizeof(dep0));
    dep0.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep0.imageMemoryBarrierCount = 1;
    dep0.pImageMemoryBarriers = &to_copy;
    vkCmdPipelineBarrier2(cmd, &dep0);

    VkBufferImageCopy region;
    memset(&region, 0, sizeof(region));
    region.bufferOffset = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = w;
    region.imageExtent.height = h;
    region.imageExtent.depth = 1;
    vkCmdCopyBufferToImage(
        cmd,
        staging,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    VkImageMemoryBarrier2 to_shader;
    memset(&to_shader, 0, sizeof(to_shader));
    to_shader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    to_shader.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    to_shader.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    to_shader.dstStageMask =
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_shader.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    to_shader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_shader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_shader.image = image;
    to_shader.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_shader.subresourceRange.baseMipLevel = 0;
    to_shader.subresourceRange.levelCount = 1;
    to_shader.subresourceRange.baseArrayLayer = 0;
    to_shader.subresourceRange.layerCount = 1;

    VkDependencyInfo dep1;
    memset(&dep1, 0, sizeof(dep1));
    dep1.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep1.imageMemoryBarrierCount = 1;
    dep1.pImageMemoryBarriers = &to_shader;
    vkCmdPipelineBarrier2(cmd, &dep1);

    end_single_use_cmd(dev, q, pool, cmd);
    vkDestroyBuffer(dev, staging, NULL);
    vkFreeMemory(dev, staging_mem, NULL);
}

/* ---------- DDS BC loader ---------- */

#define DDS_MAGIC 0x20534444u /* "DDS " */
#define DDS_FOURCC(a,b,c,d) ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24))

#define DDSD_MIPMAPCOUNT 0x00020000u
#define DDPF_FOURCC      0x00000004u

typedef struct DDS_PIXELFORMAT {
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t rgbBitCount;
    uint32_t rBitMask;
    uint32_t gBitMask;
    uint32_t bBitMask;
    uint32_t aBitMask;
} DDS_PIXELFORMAT;

typedef struct DDS_HEADER {
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserved2;
} DDS_HEADER;

typedef struct DDS_HEADER_DXT10 {
    uint32_t dxgiFormat;
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
} DDS_HEADER_DXT10;

/* DXGI_FORMAT values we need */
enum {
    DXGI_FORMAT_BC1_TYPELESS         = 70,
    DXGI_FORMAT_BC1_UNORM            = 71,
    DXGI_FORMAT_BC1_UNORM_SRGB       = 72,
    DXGI_FORMAT_BC2_TYPELESS         = 73,
    DXGI_FORMAT_BC2_UNORM            = 74,
    DXGI_FORMAT_BC2_UNORM_SRGB       = 75,
    DXGI_FORMAT_BC3_TYPELESS         = 76,
    DXGI_FORMAT_BC3_UNORM            = 77,
    DXGI_FORMAT_BC3_UNORM_SRGB       = 78,
    DXGI_FORMAT_BC4_TYPELESS         = 79,
    DXGI_FORMAT_BC4_UNORM            = 80,
    DXGI_FORMAT_BC4_SNORM            = 81,
    DXGI_FORMAT_BC5_TYPELESS         = 82,
    DXGI_FORMAT_BC5_UNORM            = 83,
    DXGI_FORMAT_BC5_SNORM            = 84
};

typedef struct DDSInfo {
    VkFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t mipLevels;
    const uint8_t* data;
    size_t dataSize;
} DDSInfo;

static int device_supports_bc(VkPhysicalDevice phys) {
    VkPhysicalDeviceFeatures features;
    memset(&features, 0, sizeof(features));
    vkGetPhysicalDeviceFeatures(phys, &features);
    return features.textureCompressionBC ? 1 : 0;
}

static int format_supports_sampled_image(VkPhysicalDevice phys, VkFormat format) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(phys, format, &props);
    return (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0;
}

static uint32_t bc_block_size_bytes(VkFormat format) {
    switch (format) {
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC4_UNORM_BLOCK:
        case VK_FORMAT_BC4_SNORM_BLOCK:
            return 8;

        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC5_UNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK:
            return 16;

        default:
            return 0;
    }
}

static size_t bc_mip_size_bytes(VkFormat format, uint32_t w, uint32_t h) {
    uint32_t blockSize = bc_block_size_bytes(format);
    uint32_t bw = (w + 3u) / 4u;
    uint32_t bh = (h + 3u) / 4u;
    return (size_t)bw * (size_t)bh * (size_t)blockSize;
}

static int dds_dxgi_to_vk(uint32_t dxgi, VkFormat* out_format) {
    switch (dxgi) {
        case DXGI_FORMAT_BC1_UNORM:      *out_format = VK_FORMAT_BC1_RGBA_UNORM_BLOCK; return 1;
        case DXGI_FORMAT_BC1_UNORM_SRGB: *out_format = VK_FORMAT_BC1_RGBA_SRGB_BLOCK;  return 1;
        case DXGI_FORMAT_BC2_UNORM:      *out_format = VK_FORMAT_BC2_UNORM_BLOCK;       return 1;
        case DXGI_FORMAT_BC2_UNORM_SRGB: *out_format = VK_FORMAT_BC2_SRGB_BLOCK;        return 1;
        case DXGI_FORMAT_BC3_UNORM:      *out_format = VK_FORMAT_BC3_UNORM_BLOCK;       return 1;
        case DXGI_FORMAT_BC3_UNORM_SRGB: *out_format = VK_FORMAT_BC3_SRGB_BLOCK;        return 1;
        case DXGI_FORMAT_BC4_UNORM:      *out_format = VK_FORMAT_BC4_UNORM_BLOCK;       return 1;
        case DXGI_FORMAT_BC4_SNORM:      *out_format = VK_FORMAT_BC4_SNORM_BLOCK;       return 1;
        case DXGI_FORMAT_BC5_UNORM:      *out_format = VK_FORMAT_BC5_UNORM_BLOCK;       return 1;
        case DXGI_FORMAT_BC5_SNORM:      *out_format = VK_FORMAT_BC5_SNORM_BLOCK;       return 1;
        default: return 0;
    }
}

static int dds_fourcc_to_vk(uint32_t fourCC, VkFormat* out_format) {
    switch (fourCC) {
        case DDS_FOURCC('D','X','T','1'):
            *out_format = VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
            return 1;
        case DDS_FOURCC('D','X','T','3'):
            *out_format = VK_FORMAT_BC2_UNORM_BLOCK;
            return 1;
        case DDS_FOURCC('D','X','T','5'):
            *out_format = VK_FORMAT_BC3_UNORM_BLOCK;
            return 1;
        case DDS_FOURCC('A','T','I','1'):
        case DDS_FOURCC('B','C','4','U'):
            *out_format = VK_FORMAT_BC4_UNORM_BLOCK;
            return 1;
        case DDS_FOURCC('B','C','4','S'):
            *out_format = VK_FORMAT_BC4_SNORM_BLOCK;
            return 1;
        case DDS_FOURCC('A','T','I','2'):
        case DDS_FOURCC('B','C','5','U'):
            *out_format = VK_FORMAT_BC5_UNORM_BLOCK;
            return 1;
        case DDS_FOURCC('B','C','5','S'):
            *out_format = VK_FORMAT_BC5_SNORM_BLOCK;
            return 1;
        default:
            return 0;
    }
}

static int parse_dds_bc(const uint8_t* bytes, size_t len, DDSInfo* out_info) {
    if (len < 4 + sizeof(DDS_HEADER)) {
        printf("DDS: too small\n");
        return 0;
    }

    uint32_t magic;
    memcpy(&magic, bytes, 4);
    if (magic != DDS_MAGIC) {
        printf("DDS: invalid magic\n");
        return 0;
    }

    DDS_HEADER hdr;
    memcpy(&hdr, bytes + 4, sizeof(hdr));

    if (hdr.size != 124) {
        printf("DDS: invalid header size: %u\n", hdr.size);
        return 0;
    }

    if (hdr.ddspf.size != 32) {
        printf("DDS: invalid pixel format size: %u\n", hdr.ddspf.size);
        return 0;
    }

    VkFormat format = VK_FORMAT_UNDEFINED;
    size_t dataOffset = 4 + sizeof(DDS_HEADER);

    if ((hdr.ddspf.flags & DDPF_FOURCC) == 0) {
        printf("DDS: expected compressed FourCC/DX10 format\n");
        return 0;
    }

    if (hdr.ddspf.fourCC == DDS_FOURCC('D','X','1','0')) {
        if (len < dataOffset + sizeof(DDS_HEADER_DXT10)) {
            printf("DDS: too small for DX10 header\n");
            return 0;
        }

        DDS_HEADER_DXT10 dx10;
        memcpy(&dx10, bytes + dataOffset, sizeof(dx10));
        dataOffset += sizeof(DDS_HEADER_DXT10);

        if (dx10.arraySize != 1) {
            printf("DDS: array textures not supported (arraySize=%u)\n", dx10.arraySize);
            return 0;
        }

        if (!dds_dxgi_to_vk(dx10.dxgiFormat, &format)) {
            printf("DDS: unsupported DXGI format: %u\n", dx10.dxgiFormat);
            return 0;
        }
    } else {
        if (!dds_fourcc_to_vk(hdr.ddspf.fourCC, &format)) {
            printf("DDS: unsupported FourCC: 0x%08X\n", hdr.ddspf.fourCC);
            return 0;
        }
    }

    uint32_t mipLevels = 1;
    if ((hdr.flags & DDSD_MIPMAPCOUNT) && hdr.mipMapCount > 0) {
        mipLevels = hdr.mipMapCount;
    }

    if (hdr.width == 0 || hdr.height == 0) {
        printf("DDS: invalid dimensions %ux%u\n", hdr.width, hdr.height);
        return 0;
    }

    size_t expected = 0;
    uint32_t w = hdr.width;
    uint32_t h = hdr.height;
    for (uint32_t i = 0; i < mipLevels; ++i) {
        expected += bc_mip_size_bytes(format, w, h);
        w = (w > 1) ? (w >> 1) : 1;
        h = (h > 1) ? (h >> 1) : 1;
    }

    if (len < dataOffset + expected) {
        printf("DDS: truncated data, expected at least %zu bytes after header, got %zu\n",
               expected, len - dataOffset);
        return 0;
    }

    out_info->format = format;
    out_info->width = hdr.width;
    out_info->height = hdr.height;
    out_info->mipLevels = mipLevels;
    out_info->data = bytes + dataOffset;
    out_info->dataSize = expected;
    return 1;
}

static int create_texture_from_dds_bc(
    struct Machine* m,
    VkCommandPool pool,
    const uint8_t* bytes,
    size_t len,
    Texture* out_tex
) {
    DDSInfo dds;
    memset(&dds, 0, sizeof(dds));

    if (!parse_dds_bc(bytes, len, &dds)) {
        printf("Failed to parse DDS\n");
        return 0;
    }

    if (!device_supports_bc(m->physical_device)) {
        printf("Device does not support BC texture compression\n");
        return 0;
    }

    if (!format_supports_sampled_image(m->physical_device, dds.format)) {
        printf("DDS: BC format not supported for sampling: %u\n", (uint32_t)dds.format);
        return 0;
    }

    VkDevice dev = m->device;
    VkPhysicalDevice phys = m->physical_device;
    VkQueue queue = m->queue_graphics;

    create_image_2d_mipped(
        dev,
        phys,
        dds.width,
        dds.height,
        dds.format,
        dds.mipLevels,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        &out_tex->image,
        &out_tex->memory
    );

    VkBuffer staging;
    VkDeviceMemory staging_mem;
    create_buffer_and_memory(
        dev,
        phys,
        (VkDeviceSize)dds.dataSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging,
        &staging_mem
    );
    upload_to_buffer(dev, staging_mem, 0, dds.data, dds.dataSize);

    VkCommandBuffer cmd = begin_single_use_cmd(dev, pool);

    VkImageMemoryBarrier2 to_copy;
    memset(&to_copy, 0, sizeof(to_copy));
    to_copy.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    to_copy.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    to_copy.srcAccessMask = 0;
    to_copy.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    to_copy.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    to_copy.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    to_copy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_copy.image = out_tex->image;
    to_copy.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_copy.subresourceRange.baseMipLevel = 0;
    to_copy.subresourceRange.levelCount = dds.mipLevels;
    to_copy.subresourceRange.baseArrayLayer = 0;
    to_copy.subresourceRange.layerCount = 1;

    VkDependencyInfo dep0;
    memset(&dep0, 0, sizeof(dep0));
    dep0.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep0.imageMemoryBarrierCount = 1;
    dep0.pImageMemoryBarriers = &to_copy;
    vkCmdPipelineBarrier2(cmd, &dep0);

    VkBufferImageCopy regions[32];
    uint32_t regionCount = 0;
    memset(regions, 0, sizeof(regions));

    size_t offset = 0;
    uint32_t w = dds.width;
    uint32_t h = dds.height;

    if (dds.mipLevels > 32) {
        printf("DDS: too many mip levels: %u\n", dds.mipLevels);
        vkDestroyImage(dev, out_tex->image, NULL);
        vkFreeMemory(dev, out_tex->memory, NULL);
        out_tex->image = VK_NULL_HANDLE;
        out_tex->memory = VK_NULL_HANDLE;
        return 0;
    }

    for (uint32_t level = 0; level < dds.mipLevels; ++level) {
        size_t mipSize = bc_mip_size_bytes(dds.format, w, h);

        VkBufferImageCopy r;
        memset(&r, 0, sizeof(r));
        r.bufferOffset = (VkDeviceSize)offset;
        r.bufferRowLength = 0;
        r.bufferImageHeight = 0;
        r.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        r.imageSubresource.mipLevel = level;
        r.imageSubresource.baseArrayLayer = 0;
        r.imageSubresource.layerCount = 1;
        r.imageOffset.x = 0;
        r.imageOffset.y = 0;
        r.imageOffset.z = 0;
        r.imageExtent.width = w;
        r.imageExtent.height = h;
        r.imageExtent.depth = 1;

        regions[regionCount++] = r;

        offset += mipSize;
        w = (w > 1) ? (w >> 1) : 1;
        h = (h > 1) ? (h >> 1) : 1;
    }

    vkCmdCopyBufferToImage(
        cmd,
        staging,
        out_tex->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        regionCount,
        regions
    );

    VkImageMemoryBarrier2 to_shader;
    memset(&to_shader, 0, sizeof(to_shader));
    to_shader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    to_shader.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    to_shader.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    to_shader.dstStageMask =
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_shader.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    to_shader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_shader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_shader.image = out_tex->image;
    to_shader.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_shader.subresourceRange.baseMipLevel = 0;
    to_shader.subresourceRange.levelCount = dds.mipLevels;
    to_shader.subresourceRange.baseArrayLayer = 0;
    to_shader.subresourceRange.layerCount = 1;

    VkDependencyInfo dep1;
    memset(&dep1, 0, sizeof(dep1));
    dep1.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep1.imageMemoryBarrierCount = 1;
    dep1.pImageMemoryBarriers = &to_shader;
    vkCmdPipelineBarrier2(cmd, &dep1);

    end_single_use_cmd(dev, queue, pool, cmd);

    vkDestroyBuffer(dev, staging, NULL);
    vkFreeMemory(dev, staging_mem, NULL);

    create_view_2d_mipped(dev, out_tex->image, dds.format, dds.mipLevels, &out_tex->view);

    return 1;
}

/* ---------- existing dummy / descriptor code ---------- */

static void create_dummy_texture(struct Machine* machine, VkCommandPool pool) {
    uint32_t pixel = 0xFFFFFFFFu;

    dummy_texture.image = VK_NULL_HANDLE;
    dummy_texture.memory = VK_NULL_HANDLE;
    dummy_texture.view = VK_NULL_HANDLE;

    create_image_2d_mipped(
        machine->device,
        machine->physical_device,
        1,
        1,
        VK_FORMAT_R8G8B8A8_UNORM,
        1,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        &dummy_texture.image,
        &dummy_texture.memory
    );
    upload_image_2d(
        machine->device,
        machine->physical_device,
        machine->queue_graphics,
        pool,
        dummy_texture.image,
        1,
        1,
        &pixel,
        sizeof(pixel)
    );
    create_view_2d_mipped(
        machine->device,
        dummy_texture.image,
        VK_FORMAT_R8G8B8A8_UNORM,
        1,
        &dummy_texture.view
    );

    for (uint32_t i = 0; i < MAX_TEXTURES; ++i) {
        textures[i] = dummy_texture;
    }

    for (uint32_t i = 0; i < MAX_DETAIL_TEXTURES; ++i) {
        detail_textures[i].image = VK_NULL_HANDLE;
        detail_textures[i].memory = VK_NULL_HANDLE;
        detail_textures[i].view = VK_NULL_HANDLE;
    }

    texture_count = 0;
}

static void create_textures(struct Machine* machine, VkCommandPool pool) {
    VkSamplerCreateInfo sci;
    memset(&sci, 0, sizeof(sci));
    sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.maxLod = 3.40282346638528859811704183484516925e+38F;
    VK_CHECK(vkCreateSampler(machine->device, &sci, NULL, &global_sampler));

    create_dummy_texture(machine, pool);

    if (create_texture_from_dds_bc(machine, pool, font_atlas, font_atlas_len, &textures[texture_count])) {
        texture_count++;
    } else {
        printf("Failed to create font atlas texture from DDS/BC\n");
    }

    if (create_texture_from_dds_bc(machine, pool, map, map_len, &textures[texture_count])) {
        texture_count++;
    } else {
        printf("Failed to create map texture from DDS/BC\n");
    }

    if (create_texture_from_dds_bc(machine, pool, height, height_len, &textures[texture_count])) {
        texture_count++;
    } else {
        printf("Failed to create height texture from DDS/BC\n");
    }

    if (create_texture_from_dds_bc(machine, pool, normals, normals_len, &textures[texture_count])) {
        texture_count++;
    } else {
        printf("Failed to create normals texture from DDS/BC\n");
    }

    if (create_texture_from_dds_bc(machine, pool, noise, noise_len, &textures[texture_count])) {
        texture_count++;
    } else {
        printf("Failed to create noise texture from DDS/BC\n");
    }

    if (create_texture_from_dds_bc(machine, pool, height_detail, height_detail_len, &textures[texture_count])) {
        texture_count++;
    } else {
        printf("Failed to create height detail texture from DDS/BC\n");
    }

    if (texture_count == 0) {
        printf("WARNING: no textures loaded; dummy will be used.\n");
    }
}

static void fill_texture_descriptor_infos(VkDescriptorImageInfo* out_infos, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        VkImageView view = textures[i].view;
        if (view == VK_NULL_HANDLE) view = dummy_texture.view;
        out_infos[i].sampler = global_sampler;
        out_infos[i].imageView = view;
        out_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
}

static void fill_detail_texture_descriptor_infos(VkDescriptorImageInfo* out_infos, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        VkImageView view = detail_textures[i].view;
        if (view == VK_NULL_HANDLE) view = dummy_texture.view;
        out_infos[i].sampler = global_sampler;
        out_infos[i].imageView = view;
        out_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
}

static int create_texture_from_r8(
    struct Machine* m,
    VkCommandPool pool,
    const uint8_t* pixels,
    uint32_t w,
    uint32_t h,
    Texture* out_tex
) {
    VkDevice dev = m->device;
    VkPhysicalDevice phys = m->physical_device;
    VkQueue queue = m->queue_graphics;

    if (out_tex->image == VK_NULL_HANDLE) {
        create_image_2d_mipped(
            dev,
            phys,
            w,
            h,
            VK_FORMAT_R8_UNORM,
            1,
            VK_IMAGE_USAGE_SAMPLED_BIT,
            &out_tex->image,
            &out_tex->memory
        );

        create_view_2d_mipped(
            dev,
            out_tex->image,
            VK_FORMAT_R8_UNORM,
            1,
            &out_tex->view
        );
    }

    upload_image_2d(
        dev,
        phys,
        queue,
        pool,
        out_tex->image,
        w,
        h,
        pixels,
        (VkDeviceSize)(w * h)
    );

    return 1;
}

static int upload_detail_texture_pair(
    struct Machine* machine,
    VkCommandPool pool,
    const uint8_t* terrain_pixels,
    const uint8_t* height_pixels,
    uint32_t w,
    uint32_t h
) {
    if (!create_texture_from_r8(
            machine, pool,
            terrain_pixels, w, h,
            &detail_textures[DETAIL_TEXTURE_TERRAIN])) {
        printf("Failed to upload terrain detail texture\n");
        return 0;
    }

    if (!create_texture_from_r8(
            machine, pool,
            height_pixels, w, h,
            &detail_textures[DETAIL_TEXTURE_HEIGHT])) {
        printf("Failed to upload height detail texture\n");
        return 0;
    }

    return 1;
}
