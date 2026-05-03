// // 19 buffers
// VkBuffer buffer_chunks; VkDeviceMemory memory_chunks;
// VkBuffer buffer_visible_chunk_ids; VkDeviceMemory memory_visible_chunk_ids;
// VkBuffer buffer_indirect_workgroups; VkDeviceMemory memory_indirect_workgroups;
// VkBuffer buffer_visible_object_count; VkDeviceMemory memory_visible_object_count;
// VkBuffer buffer_objects; VkDeviceMemory memory_objects;
// VkBuffer buffer_count_per_mesh; VkDeviceMemory memory_count_per_mesh;
// VkBuffer buffer_offset_per_mesh; VkDeviceMemory memory_offset_per_mesh;
// VkBuffer buffer_visible_ids; VkDeviceMemory memory_visible_ids;
// VkBuffer buffer_visible; VkDeviceMemory memory_visible;
// VkBuffer buffer_object_mesh_list; VkDeviceMemory memory_object_mesh_list;
// VkBuffer buffer_object_metadata; VkDeviceMemory memory_object_metadata;
// VkBuffer buffer_mesh_info; VkDeviceMemory memory_mesh_info;
// VkBuffer buffer_draw_calls; VkDeviceMemory memory_draw_calls;
// VkBuffer buffer_positions; VkDeviceMemory memory_positions;
// VkBuffer buffer_normals; VkDeviceMemory memory_normals;
// VkBuffer buffer_uvs; VkDeviceMemory memory_uvs;
// VkBuffer buffer_index_ib; VkDeviceMemory memory_indices;
// VkBuffer buffer_buckets; VkDeviceMemory memory_buckets;
// VkBuffer buffer_units; VkDeviceMemory memory_units;
// // 1 uniform buffer
// VkBuffer buffer_uniforms; VkDeviceMemory memory_uniforms;
// // plus two texture bindings == 21 bindings total
//
//
// // input:
// // description of which buffers, textures and uniforms we want
// //
//
//
// void create_buffers(
//     VkDevice device, VkPhysicalDevice physical_device,
//     VkQueue queue_graphics,
//     unsigned int queue_family_graphics
//     )
// {
//     VkCommandPoolCreateInfo pool_info = {
//         .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
//         .queueFamilyIndex = queue_family_graphics, // use your graphics family index
//         .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
//     };
//     VkCommandPool upload_pool; VK_CHECK(vkCreateCommandPool(device, &pool_info, NULL, &upload_pool));
//
//     // VISIBLE CHUNK IDS
//     create_buffer_and_memory(device, physical_device, size_visible_chunk_ids,
//         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer_visible_chunk_ids, &memory_visible_chunk_ids);
//
//     // INDIRECT WORKGROUPS
//     create_buffer_and_memory(device, physical_device, size_indirect_workgroups,
//         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
//         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer_indirect_workgroups, &memory_indirect_workgroups);
//
//     // VISIBLE OBJECT COUNT
//     create_buffer_and_memory(device, physical_device, size_visible_object_count,
//         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer_visible_object_count, &memory_visible_object_count);
//
//     // COUNT PER MESH
//     create_buffer_and_memory(device, physical_device, size_count_per_mesh,
//         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer_count_per_mesh, &memory_count_per_mesh);
//
//     // OFFSET PER MESH
//     create_buffer_and_memory(device, physical_device, size_offset_per_mesh,
//         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer_offset_per_mesh, &memory_offset_per_mesh);
//
//     // VISIBLE OBJECT IDS
//     create_buffer_and_memory(device, physical_device, size_visible_ids,
//         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer_visible_ids, &memory_visible_ids);
//
//     // RENDERED INSTANCES
//     create_buffer_and_memory(device, physical_device, size_rendered_instances,
//         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer_visible, &memory_visible);
//
//     // DRAW_CALLS
//     create_buffer_and_memory(device, physical_device, size_draw_calls,
//         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer_draw_calls, &memory_draw_calls);
//
//     // BUCKETS
//     create_buffer_and_memory(device, physical_device, size_buckets,
//         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer_buckets, &memory_buckets);
//
//     // UNIFORMS (host visible)
//     create_buffer_and_memory(device, physical_device, sizeof(struct uniforms),
//         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//         &buffer_uniforms, &memory_uniforms);
//
//     // UNITS
//     create_and_upload_device_local_buffer(
//         device, physical_device, queue_graphics, upload_pool,
//         size_units, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//         units, // uploaded immediately here
//         &buffer_units, &memory_units, 0);
//
//     // OBJECT MESH LIST
//     create_and_upload_device_local_buffer(
//         device, physical_device, queue_graphics, upload_pool,
//         size_object_mesh_list, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
//         object_mesh_offsets, // uploaded immediately here
//         &buffer_object_mesh_list, &memory_object_mesh_list, 0);
//
//     // OBJECT METADATA
//     create_and_upload_device_local_buffer(
//         device, physical_device, queue_graphics, upload_pool,
//         size_object_metadata, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
//         object_metadata, // uploaded immediately here
//         &buffer_object_metadata, &memory_object_metadata, 0);
//
//     // MESH INFO / POSITIONS / NORMALS / UVS / INDICES / CHUNKS / OBJECTS
//     create_and_upload_device_local_buffer(
//         device, physical_device, queue_graphics, upload_pool,
//         size_mesh_info, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
//         mesh_info, // uploaded immediately here
//         &buffer_mesh_info, &memory_mesh_info, 0);
//     create_and_upload_device_local_buffer(
//         device, physical_device, queue_graphics, upload_pool,
//         size_positions, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
//         NULL, // uploaded later per mesh
//         &buffer_positions, &memory_positions, 0);
//     create_and_upload_device_local_buffer(
//         device, physical_device, queue_graphics, upload_pool,
//         size_normals, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
//         NULL, // uploaded later per mesh
//         &buffer_normals, &memory_normals, 0);
//     create_and_upload_device_local_buffer(
//         device, physical_device, queue_graphics, upload_pool,
//         size_uvs, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
//         NULL, // uploaded later per mesh
//         &buffer_uvs, &memory_uvs, 0);
//     create_and_upload_device_local_buffer(
//         device, physical_device, queue_graphics, upload_pool,
//         size_indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
//         NULL, // uploaded later per mesh
//         &buffer_index_ib, &memory_indices, 0);
//     create_and_upload_device_local_buffer(
//         device, physical_device, queue_graphics, upload_pool,
//         size_chunks, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
//         NULL, // uploaded later per mesh
//         &buffer_chunks, &memory_chunks, 0);
//     create_and_upload_device_local_buffer(
//         device, physical_device, queue_graphics, upload_pool,
//         size_objects, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//         NULL, // uploaded later per mesh
//         &buffer_objects, &memory_objects, 0);
//
//     // readback object id
//     create_buffer_and_memory(
//         device, physical_device,
//         sizeof(uint32_t),
//         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//         &swapchain.pick_readback_buffer, &swapchain.pick_readback_memory
//     );
//     // readback a region
//     uint32_t pick_width  = swapchain.swapchain_extent.width;
//     uint32_t pick_height = swapchain.swapchain_extent.height;
//     VkDeviceSize pick_max_bytes = (VkDeviceSize)pick_width * pick_height * sizeof(uint32_t);
//     create_buffer_and_memory(
//         device, physical_device,
//         pick_max_bytes,
//         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//         &swapchain.pick_region_buffer, &swapchain.pick_region_memory);
//     // readback depth
//     create_buffer_and_memory(
//         device, physical_device,
//         sizeof(float),
//         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//         &swapchain.depth_readback_buffer, &swapchain.depth_readback_memory);
//
//     pf_timestamp("Buffers created");
//
//     // upload the data per mesh (later, when data is huge, batch into one big command to upload everything)
//     mesh_index = 0;
//     for (int m = 0; m < MESH_TYPE_COUNT; ++m) {
//         int anim_count = meshes[m].num_animations == 0 ? 1 : meshes[m].num_animations; // special case for no animations
//         int frame_count = meshes[m].num_animations == 0 ? 1 : ANIMATION_FRAMES; // special case for no animations to one frame
//         for (int a = 0; a < anim_count; ++a) {
//             for (int f = 0; f < frame_count; ++f) {
//                 for (int lod = 0; lod < LOD_LEVELS; ++lod) {
//                     // vertices
//                     if (meshes[m].animations[a].frames[f].lods[lod].positions) {
//                         VkDeviceSize bytes = meshes[m].lods[lod].num_vertices * sizeof(uint32_t);
//                         VkDeviceSize dstOfs = (VkDeviceSize)mesh_info[mesh_index].base_vertex * sizeof(uint32_t);
//                         // Create a temp staging and copy into the already-created DEVICE_LOCAL buffer at dstOfs
//                         // Reuse the helper by creating a temp device-local “alias” of same buffer & memory? Simpler: a small inline staging+copy:
//                         {
//                             VkBuffer staging; VkDeviceMemory staging_mem;
//                             create_buffer_and_memory(device, physical_device, bytes,
//                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//                                 &staging, &staging_mem);
//                             upload_to_buffer(device, staging_mem, 0, meshes[m].animations[a].frames[f].lods[lod].positions, (size_t)bytes);
//                             VkCommandBuffer cmd = begin_single_use_cmd(device, upload_pool);
//                             VkBufferCopy c = { .srcOffset = 0, .dstOffset = dstOfs, .size = bytes };
//                             vkCmdCopyBuffer(cmd, staging, buffer_positions, 1, &c);
//                             end_single_use_cmd(device, queue_graphics, upload_pool, cmd);
//                             vkDestroyBuffer(device, staging, NULL);
//                             vkFreeMemory(device, staging_mem, NULL);
//                         }
//                     }
//                     // normals
//                     if (meshes[m].animations[a].frames[f].lods[lod].normals) {
//                         VkDeviceSize bytes = meshes[m].lods[lod].num_vertices * sizeof(uint32_t);
//                         VkDeviceSize dstOfs = (VkDeviceSize)mesh_info[mesh_index].base_vertex * sizeof(uint32_t);
//                         VkBuffer staging; VkDeviceMemory staging_mem;
//                         create_buffer_and_memory(device, physical_device, bytes,
//                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//                             &staging, &staging_mem);
//                         upload_to_buffer(device, staging_mem, 0, meshes[m].animations[a].frames[f].lods[lod].normals, (size_t)bytes);
//                         VkCommandBuffer cmd = begin_single_use_cmd(device, upload_pool);
//                         VkBufferCopy c = { .srcOffset = 0, .dstOffset = dstOfs, .size = bytes };
//                         vkCmdCopyBuffer(cmd, staging, buffer_normals, 1, &c);
//                         end_single_use_cmd(device, queue_graphics, upload_pool, cmd);
//                         vkDestroyBuffer(device, staging, NULL);
//                         vkFreeMemory(device, staging_mem, NULL);
//                     }
//                     // uvs
//                     if (meshes[m].lods[lod].uvs) {
//                         VkDeviceSize bytes = meshes[m].lods[lod].num_vertices * sizeof(uint32_t);
//                         VkDeviceSize dstOfs = (VkDeviceSize)mesh_info[mesh_index].base_vertex * sizeof(uint32_t);
//                         VkBuffer staging; VkDeviceMemory staging_mem;
//                         create_buffer_and_memory(device, physical_device, bytes,
//                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//                             &staging, &staging_mem);
//                         upload_to_buffer(device, staging_mem, 0, meshes[m].lods[lod].uvs, (size_t)bytes);
//                         VkCommandBuffer cmd = begin_single_use_cmd(device, upload_pool);
//                         VkBufferCopy c = { .srcOffset = 0, .dstOffset = dstOfs, .size = bytes };
//                         vkCmdCopyBuffer(cmd, staging, buffer_uvs, 1, &c);
//                         end_single_use_cmd(device, queue_graphics, upload_pool, cmd);
//                         vkDestroyBuffer(device, staging, NULL);
//                         vkFreeMemory(device, staging_mem, NULL);
//                     }
//                     // indices
//                     {
//                         VkDeviceSize bytes = meshes[m].lods[lod].num_indices * sizeof(uint16_t);
//                         VkDeviceSize dstOfs = (VkDeviceSize)mesh_info[mesh_index].first_index * sizeof(uint16_t);
//                         VkBuffer staging; VkDeviceMemory staging_mem;
//                         create_buffer_and_memory(device, physical_device, bytes,
//                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//                             &staging, &staging_mem);
//                         upload_to_buffer(device, staging_mem, 0, meshes[m].lods[lod].indices, (size_t)bytes);
//                         VkCommandBuffer cmd = begin_single_use_cmd(device, upload_pool);
//                         VkBufferCopy c = { .srcOffset = 0, .dstOffset = dstOfs, .size = bytes };
//                         vkCmdCopyBuffer(cmd, staging, buffer_index_ib, 1, &c);
//                         end_single_use_cmd(device, queue_graphics, upload_pool, cmd);
//                         vkDestroyBuffer(device, staging, NULL);
//                         vkFreeMemory(device, staging_mem, NULL);
//                     }
//                     mesh_index++;
//                 }
//             }
//         }
//     }
//
//     // chunks
//     {
//         VkDeviceSize bytes = TOTAL_CHUNK_COUNT * sizeof(struct gpu_chunk);
//         VkDeviceSize dstOfs = (VkDeviceSize)0 * sizeof(struct gpu_chunk);
//         VkBuffer staging; VkDeviceMemory staging_mem;
//         create_buffer_and_memory(device, physical_device, bytes,
//             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//             &staging, &staging_mem);
//         upload_to_buffer(device, staging_mem, 0, gpu_chunks, (size_t)bytes);
//         VkCommandBuffer cmd = begin_single_use_cmd(device, upload_pool);
//         VkBufferCopy c = { .srcOffset = 0, .dstOffset = dstOfs, .size = bytes };
//         vkCmdCopyBuffer(cmd, staging, buffer_chunks, 1, &c);
//         end_single_use_cmd(device, queue_graphics, upload_pool, cmd);
//         vkDestroyBuffer(device, staging, NULL);
//         vkFreeMemory(device, staging_mem, NULL);
//     }
//
//     // objects
//     {
//         VkDeviceSize bytes = TOTAL_CHUNK_COUNT * OBJECTS_PER_CHUNK * sizeof(struct gpu_object);
//         VkDeviceSize dstOfs = (VkDeviceSize)0 * OBJECTS_PER_CHUNK * sizeof(struct gpu_object);
//         VkBuffer staging; VkDeviceMemory staging_mem;
//         create_buffer_and_memory(device, physical_device, bytes,
//             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//             &staging, &staging_mem);
//         upload_to_buffer(device, staging_mem, 0, gpu_objects, (size_t)bytes);
//         VkCommandBuffer cmd = begin_single_use_cmd(device, upload_pool);
//         VkBufferCopy c = { .srcOffset = 0, .dstOffset = dstOfs, .size = bytes };
//         vkCmdCopyBuffer(cmd, staging, buffer_objects, 1, &c);
//         end_single_use_cmd(device, queue_graphics, upload_pool, cmd);
//         vkDestroyBuffer(device, staging, NULL);
//         vkFreeMemory(device, staging_mem, NULL);
//     }
//
//     // UPLOAD TEXTURES
//     create_textures(&machine, &swapchain);
//     create_detail_region(531, 1041); // fills in the arrays
//     upload_detail_texture_pair(&machine, &swapchain,
//         g_detail_terrain, g_detail_height, DETAIL_UPSCALED_W, DETAIL_UPSCALED_H);
//
//     pf_timestamp("Uploads complete");
// }
//
