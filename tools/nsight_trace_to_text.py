#!/usr/bin/env python3
"""
nsight_trace_to_text.py — Convert NVIDIA Nsight Graphics GPU Trace (.ngfx-gputrace)
                          into a comprehensive, AI-readable text file.

The .ngfx-gputrace format is a proprietary NVIDIA binary format (WRPV header +
protobuf-like encoded metadata + binary profiling data). This script extracts
every readable piece of information through multi-pass binary analysis.

Usage:
    python nsight_trace_to_text.py <input.ngfx-gputrace> [--output output.txt] [--verbosity summary|normal|verbose]

Requires: Python 3.8+ (no external dependencies)
"""

import argparse
import os
import re
import struct
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


# ─── Data Classes ──────────────────────────────────────────────────────────────

@dataclass
class TraceHeader:
    magic: str = ""
    version: int = 0
    section_count: int = 0
    file_size: int = 0
    raw_header: bytes = b""
    metadata_offset: int = 0
    metadata_size: int = 0
    data_offset: int = 0
    data_size: int = 0
    api_call_offset: int = 0


@dataclass
class SystemInfo:
    computer_name: str = ""
    operating_system: str = ""
    os_build: str = ""
    processor: str = ""
    ram_usage: str = ""
    gpu_device: str = ""
    gpu_chip: str = ""
    driver_version: str = ""
    hardware_scheduling: str = ""
    vram_requested: str = ""
    vram_committed: str = ""
    bytes_demoted: str = ""


@dataclass
class CaptureSettings:
    product_version: str = ""
    executable_path: str = ""
    command_line: str = ""
    graphics_api: str = ""
    troubleshooting_controls: str = ""
    start_after: str = ""
    max_duration: str = ""
    limited_to: str = ""
    allocated_event_memory: str = ""
    vsync_mode: str = ""
    gpu_clocks: str = ""
    screenshot_enabled: str = ""
    time_every_action: str = ""
    trace_shader_bindings: str = ""
    pipeline_collection: str = ""
    debug_info: str = ""
    nvtx_ranges: str = ""
    recompile_cached: str = ""
    shader_modules: str = ""


@dataclass
class ProfilingMetadata:
    sample_duration: str = ""
    warp_state_info: str = ""
    sampling_interval: str = ""
    pma_buffer_size: str = ""


@dataclass
class ExtractedData:
    header: TraceHeader = field(default_factory=TraceHeader)
    system_info: SystemInfo = field(default_factory=SystemInfo)
    capture_settings: CaptureSettings = field(default_factory=CaptureSettings)
    profiling_metadata: ProfilingMetadata = field(default_factory=ProfilingMetadata)
    vulkan_api_calls: list = field(default_factory=list)
    vulkan_enums: list = field(default_factory=list)
    pipeline_stages: list = field(default_factory=list)
    image_layouts: list = field(default_factory=list)
    access_flags: list = field(default_factory=list)
    shader_info: list = field(default_factory=list)
    resource_handles: list = field(default_factory=list)
    memory_barriers: list = field(default_factory=list)
    draw_calls: list = field(default_factory=list)
    dispatches: list = field(default_factory=list)
    synchronization: list = field(default_factory=list)
    nvtx_markers: list = field(default_factory=list)
    processes: list = field(default_factory=list)
    swapchain_info: list = field(default_factory=list)
    format_info: list = field(default_factory=list)
    present_modes: list = field(default_factory=list)
    all_categorized_strings: dict = field(default_factory=dict)
    layout_info: dict = field(default_factory=dict)


# ─── Layout File Parsing ──────────────────────────────────────────────────────

def parse_layout_file(layout_path: Path) -> dict:
    """Parse Nsight .layout JSON file if present next to trace file."""
    if not layout_path.exists():
        return {}
    import json
    try:
        with open(layout_path, 'r', encoding='utf-8', errors='replace') as f:
            data = json.load(f)

        def extract_keys(obj):
            keys = []
            if isinstance(obj, dict):
                if 'key' in obj and isinstance(obj['key'], str):
                    keys.append(obj['key'])
                for v in obj.values():
                    keys.extend(extract_keys(v))
            elif isinstance(obj, list):
                for item in obj:
                    keys.extend(extract_keys(item))
            return keys

        keys = sorted(set(extract_keys(data)))
        fg = data.get('warp_flame_graph_view', {})
        visible_range = (fg.get('visibleRangeStart'), fg.get('visibleRangeEnd'))
        el = data.get('warp_event_list_view', {}).get('event_list_expanded', [])
        expanded_indices = [item['Index'] for item in el if isinstance(item, dict) and 'Index' in item]

        return {
            'layout_file': layout_path.name,
            'captured_metrics': keys,
            'visible_range': visible_range,
            'expanded_events': expanded_indices,
        }
    except Exception:
        return {}



# ─── Header Parsing ───────────────────────────────────────────────────────────

def parse_header(data: bytes) -> TraceHeader:
    """Parse the WRPV file header and identify section offsets."""
    header = TraceHeader()
    header.file_size = len(data)

    if len(data) < 64:
        print("ERROR: File too small to be a valid .ngfx-gputrace", file=sys.stderr)
        sys.exit(1)

    # Magic: "WRPV" (4 bytes)
    header.magic = data[:4].decode('ascii', errors='replace')
    if header.magic != "WRPV":
        print(f"WARNING: Unexpected magic '{header.magic}' (expected 'WRPV')", file=sys.stderr)

    header.raw_header = data[:64]

    # Parse header fields (little-endian)
    # Bytes 4-7: newline + padding
    # Bytes 8-15: appears to be a section size or version
    # Bytes 16-23: section count (as uint64)
    # Bytes 24-31, 32-39, 40-47, 48-55: section offsets/sizes
    try:
        vals = struct.unpack('<4sI Q Q Q Q Q', data[:60])
        header.version = vals[1]
        header.section_count = vals[2]
        header.metadata_offset = 56  # Metadata starts right after header area
        # Offsets from header
        header.data_offset = vals[3] if vals[3] < len(data) else 0
        header.data_size = vals[4] if vals[4] < len(data) else 0
    except struct.error:
        pass

    # Find where the API call section starts by searching for known API patterns
    api_start = data.find(b'vkQueueSubmit')
    if api_start > 0:
        # Back up to find the section boundary
        header.api_call_offset = max(0, api_start - 200)

    return header


# ─── Protobuf-like Metadata Extraction ─────────────────────────────────────────

def extract_protobuf_strings(data: bytes, start: int, end: int, depth: int = 0) -> list:
    """
    Extract key-value pairs from protobuf-like encoded data.
    The format uses length-prefixed strings with tag bytes:
    - 0x0a = field tag (length-delimited string, field number varies)
    - 0x12 = nested field tag
    - Length byte follows, then the string content
    """
    MAX_DEPTH = 10
    if depth > MAX_DEPTH:
        return []

    results = []
    pos = start

    while pos < end and pos < len(data):
        byte = data[pos]

        # Look for length-delimited fields (wire type 2, tag & 0x07 == 2)
        if (byte & 0x07) == 2:
            field_num = byte >> 3
            pos += 1
            if pos >= end:
                break

            # Read varint length
            length = 0
            shift = 0
            while pos < end and shift < 35:  # varint can't exceed 5 bytes for uint32
                b = data[pos]
                length |= (b & 0x7f) << shift
                pos += 1
                shift += 7
                if (b & 0x80) == 0:
                    break

            if length > 0 and length < 10000 and pos + length <= len(data):
                chunk = data[pos:pos + length]
                # Check if it's a readable ASCII/UTF-8 string
                try:
                    text = chunk.decode('utf-8', errors='strict')
                    if text.isprintable() and len(text.strip()) > 0:
                        results.append((field_num, text.strip()))
                except (UnicodeDecodeError, ValueError):
                    # Might be a nested message, try to recurse (with depth limit)
                    if depth < MAX_DEPTH:
                        try:
                            nested = extract_protobuf_strings(data, pos, pos + length, depth + 1)
                            results.extend(nested)
                        except RecursionError:
                            pass
                pos += length
            else:
                # Skip invalid lengths
                if 0 < length < 10000:
                    pos += length
                else:
                    pos += 1
        else:
            pos += 1

    return results


def parse_metadata_section(data: bytes) -> tuple:
    """Parse the structured metadata section (system info, capture settings)."""
    system_info = SystemInfo()
    capture_settings = CaptureSettings()
    profiling_meta = ProfilingMetadata()

    # The metadata is in the first ~2000 bytes after the header
    # Extract using protobuf-like parsing
    raw_pairs = extract_protobuf_strings(data, 56, min(len(data), 10000))

    # Also do direct regex extraction for known patterns (more reliable)
    metadata_region = data[:10000]

    # System info
    def find_value_after(key: bytes, region: bytes, max_gap: int = 40) -> str:
        idx = region.find(key)
        if idx < 0:
            return ""
        after = region[idx + len(key):idx + len(key) + max_gap]
        # Extract the next readable string
        m = re.search(rb'[\x20-\x7e]{2,100}', after)
        if m:
            return m.group().decode('ascii', errors='replace').strip()
        return ""

    # Parse profiling header across wider region
    prof_region = data[:50000].decode('ascii', errors='replace')
    m = re.search(r'Sample duration\s*=\s*(\d+[a-z]+)', prof_region)
    if m:
        profiling_meta.sample_duration = m.group(1)
    else:
        m = re.search(r'Sample dur[^\n\r=]*=\s*(\d+[a-z]+)', prof_region)
        if m:
            profiling_meta.sample_duration = m.group(1)

    m = re.search(r'Warp State\s*(%?)', prof_region)
    if m:
        profiling_meta.warp_state_info = "Warp State sampling enabled"

    m = re.search(r'(?:Sampling\s+|ing\s+)?Interval\s*=\s*(\d+)', prof_region)
    if m:
        profiling_meta.sampling_interval = m.group(1) + " cycles"

    m = re.search(r'PMA Buffer Size \(MB\)\s*=\s*(\d+)', prof_region)
    if m:
        profiling_meta.pma_buffer_size = m.group(1) + " MB"

    metadata_region = data[:50000]

    # System info extraction with byte-level search
    system_info.computer_name = find_value_after(b'Computer Name', metadata_region)
    system_info.gpu_device = find_value_after(b'Device', metadata_region, 60)

    # More robust extraction using readable strings
    all_strings = []
    for m_obj in re.finditer(rb'[\x20-\x7e]{3,200}', metadata_region):
        all_strings.append((m_obj.start(), m_obj.group().decode('ascii', errors='replace')))

    for i, (pos, s) in enumerate(all_strings):
        s_lower = s.lower().strip()
        next_val = all_strings[i + 1][1] if i + 1 < len(all_strings) else ""

        if 'computer name' in s_lower:
            system_info.computer_name = next_val
        elif s_lower.startswith('operating') or 'operating' in s_lower:
            for j in range(i + 1, min(i + 3, len(all_strings))):
                if 'windows' in all_strings[j][1].lower():
                    system_info.operating_system = all_strings[j][1]
                    break
        elif 'build' in s_lower and not system_info.os_build:
            system_info.os_build = next_val
        elif 'processor' in s_lower:
            proc_region = metadata_region[pos:pos + 120]
            proc_strings = re.findall(rb'[\x20-\x7e]{5,80}', proc_region)
            for ps in proc_strings[1:]:
                decoded = ps.decode('ascii', errors='replace').strip()
                if any(k in decoded.lower() for k in ['amd', 'intel', 'ryzen', 'core']):
                    system_info.processor = decoded.strip('"').strip(' $')
                    break
        elif 'ram usage' in s_lower:
            system_info.ram_usage = next_val
        elif s_lower.strip() == 'device' and not system_info.gpu_device:
            for j in range(i + 1, min(i + 4, len(all_strings))):
                val = all_strings[j][1]
                if any(k in val for k in ['NVIDIA', 'AMD', 'Intel', 'GeForce', 'Radeon']):
                    system_info.gpu_device = val
                    break
        elif 'chip' in s_lower and not system_info.gpu_chip:
            system_info.gpu_chip = next_val
        elif 'driver version' in s_lower:
            system_info.driver_version = next_val
        elif 'hardware scheduling' in s_lower:
            system_info.hardware_scheduling = next_val
        elif 'vram requested' in s_lower:
            system_info.vram_requested = next_val
        elif 'bytes demoted' in s_lower:
            system_info.bytes_demoted = next_val
        elif 'product' in s_lower and not capture_settings.product_version:
            for j in range(i + 1, min(i + 3, len(all_strings))):
                val = all_strings[j][1]
                if re.search(r'\d{4}\.\d', val):
                    capture_settings.product_version = val.strip('"').strip(',').strip()
                    break
        elif s_lower == 'vulkan':
            capture_settings.graphics_api = 'Vulkan'
        elif 'command line' in s_lower:
            capture_settings.command_line = next_val
        elif 'start after' in s_lower:
            capture_settings.start_after = next_val
        elif 'max d' in s_lower:
            capture_settings.max_duration = next_val
        elif 'limited to' in s_lower:
            capture_settings.limited_to = next_val
        elif 'memory (kb)' in s_lower:
            capture_settings.allocated_event_memory = next_val + " kB"
        elif 'v-sync mode' in s_lower:
            capture_settings.vsync_mode = next_val
        elif 'gpu clocks' in s_lower:
            capture_settings.gpu_clocks = next_val

    # Clean executable path matching
    exe_match = re.search(rb'[A-Z]:\\[A-Za-z0-9_ \-\\]+\.exe', metadata_region)
    if exe_match:
        capture_settings.executable_path = exe_match.group().decode('ascii', errors='replace')
    else:
        path_match = re.search(rb'[A-Z]:\\[\x20-\x7e]{10,200}\.exe', metadata_region)
        if path_match:
            capture_settings.executable_path = path_match.group().decode('ascii', errors='replace')


    return system_info, capture_settings, profiling_meta


# ─── Comprehensive String Extraction & Categorization ──────────────────────────

PNG_NOISE_TOKENS = {'IDAT', 'IHDR', 'PLTE', 'GAMA', 'PHYS', 'TIME', 'BKGD', 'CHRM', 'SRGB', 'ICCP', 'TEXT', 'ZTXT', 'ITXT', 'IEND'}

def extract_and_categorize_strings(data: bytes) -> dict:
    """Extract all readable strings from the binary and categorize them."""
    categories = defaultdict(list)
    seen = set()

    for m in re.finditer(rb'[\x20-\x7e]{4,500}', data):
        s = m.group().decode('ascii', errors='replace').strip()
        pos = m.start()

        if s in seen or len(s) < 4:
            continue
        seen.add(s)

        entry = (pos, s)

        s_upper = s.upper()
        s_lower = s.lower()

        # Filter PNG chunk noise
        if any(s_upper.startswith(p) for p in PNG_NOISE_TOKENS) or s_upper in PNG_NOISE_TOKENS:
            continue

        # Vulkan API calls
        if re.match(r'^vk[A-Z][A-Za-z0-9]+$', s) and len(s) >= 5:
            categories['vulkan_api_calls'].append(entry)
        # Vulkan enums/constants
        elif s.startswith('VK_') or s.startswith('VkPipeline') or s.startswith('VkImage') or s.startswith('VkFence') or (s.startswith('Vk') and len(s) > 5 and s[2].isupper()):
            categories['vulkan_enums'].append(entry)
        # Pipeline stages
        elif 'PIPELINE_STAGE' in s_upper or 'STAGE_2_' in s_upper:
            categories['pipeline_stages'].append(entry)
        # Access flags
        elif 'ACCESS_2' in s_upper or ('ACCESS' in s_upper and ('READ' in s_upper or 'WRITE' in s_upper)):
            categories['access_flags'].append(entry)
        # Image layouts
        elif 'IMAGE_LAYOUT' in s_upper or ('LAYOUT' in s_upper and 'OPTIMAL' in s_upper):
            categories['image_layouts'].append(entry)
        # VK formats
        elif 'VK_FORMAT' in s_upper or ('FORMAT_' in s_upper and ('SRGB' in s_upper or 'UNORM' in s_upper or 'UINT' in s_upper or 'SFLOAT' in s_upper)):
            categories['format_info'].append(entry)
        # Present modes
        elif 'PRESENT_MODE' in s_upper or 'PRESENT_SRC' in s_upper:
            categories['present_modes'].append(entry)
        # Draw/dispatch commands
        elif 'DRAW' in s_upper and ('INDIRECT' in s_upper or 'INDEX' in s_upper or 'COUNT' in s_upper):
            categories['draw_calls'].append(entry)
        elif 'Dispatch' in s or 'dispatch' in s_lower:
            categories['dispatches'].append(entry)
        # Barriers/synchronization
        elif 'Barrier' in s or 'BARRIER' in s_upper:
            categories['barriers'].append(entry)
        elif 'Semaphore' in s or 'semaphore' in s_lower or 'fence' in s_lower:
            categories['synchronization'].append(entry)
        # Shader info
        elif any(k in s_upper for k in ['VK_SHADER_', 'VERTEX_SHADER', 'FRAGMENT_SHADER', 'COMPUTE_SHADER', 'MESH_SHADER', 'TASK_SHADER']):
            categories['shader_info'].append(entry)
        # Descriptor/binding info
        elif any(k in s for k in ['Descriptor', 'BindPoint', 'BIND_POINT', 'firstSet', 'DescriptorSets']):
            categories['descriptor_info'].append(entry)
        # Memory/buffer info
        elif any(k in s for k in ['buffer', 'Buffer', 'memory', 'Memory', 'Offset', 'stride', 'size']):
            categories['memory_buffer_info'].append(entry)
        # NVTX markers
        elif 'NVTX' in s_upper or 'Domain' in s or 'Marker' in s:
            categories['nvtx_markers'].append(entry)
        # Handles (hex addresses)
        elif re.match(r'^0x[0-9a-fA-F]{8,16}$', s):
            categories['resource_handles'].append(entry)
        # System/process info
        elif s.endswith('.exe') or s.endswith('.dll'):
            name_part = s[:-4] if len(s) > 4 else ''
            if name_part and any(c.isalnum() for c in name_part):
                categories['processes'].append(entry)
        # File paths
        elif ':\\' in s and len(s) > 6:
            categories['file_paths'].append(entry)
        elif '/' in s and len(s) > 10:
            alpha_ratio = sum(1 for c in s if c.isalpha()) / len(s)
            if alpha_ratio > 0.5:
                categories['file_paths'].append(entry)
        # Profiling/counter related
        elif any(k in s_lower for k in ['sample duration', 'warp state', 'metric', 'counter', 'interval', 'pma buffer']):
            categories['profiling_data'].append(entry)
        # Swapchain
        elif any(k in s_lower for k in ['swapchain', 'present']):
            categories['swapchain_info'].append(entry)
        # Parameter names (API call parameters)
        elif re.match(r'^[a-z][A-Za-z0-9]{4,30}$', s) and not s.startswith('vk'):
            categories['api_parameters'].append(entry)
        # Data types
        elif s in ('uint32_t', 'uint64_t', 'int32_t', 'float', 'const', 'void*'):
            categories['data_types'].append(entry)
        # Named constants — strict filter
        elif re.match(r'^[A-Z][A-Z0-9_]{3,40}$', s):
            known_prefixes = ('VK_', 'ALL_', 'DRAW_', 'ONLY_', 'DEPTH_', 'COLOR_',
                              'BOTTOM_', 'TOP_', 'TRANSFER', 'UNDEFINED', 'GRAPHICS',
                              'COMPUTE', 'PRESENT', 'STENCIL', 'OPTIMAL', 'GENERAL',
                              'NONE', 'SM', 'GPU', 'CPU', 'PMA', 'NVTX', 'FECS',
                              'EARLY_', 'LATE_')
            has_underscore = '_' in s
            is_known = any(s.startswith(p) for p in known_prefixes)
            # Avoid single trailing underscore noise like 'DW_Q', 'SS_Q'
            if not s.endswith('_Q') and not s.endswith('_') and (has_underscore or is_known):
                categories['named_constants'].append(entry)
        # Meaningful strings
        elif len(s) >= 6 and sum(1 for c in s if c.isalpha()) >= 4 and not re.search(r'[^a-zA-Z0-9_\-\.\:\/\\]', s):
            categories['other_strings'].append(entry)

    return dict(categories)



# ─── API Call Reconstruction ───────────────────────────────────────────────────

# Known Vulkan API function names for better matching
KNOWN_VK_FUNCTIONS = [
    'vkQueueSubmit', 'vkQueueSubmit2', 'vkQueuePresentKHR', 'vkQueueWaitIdle',
    'vkCmdPipelineBarrier', 'vkCmdPipelineBarrier2', 'vkCmdPipelineBarrier2KHR',
    'vkCmdBindDescriptorSets', 'vkCmdPushDescriptorSet',
    'vkCmdDraw', 'vkCmdDrawIndexed', 'vkCmdDrawIndirect', 'vkCmdDrawIndexedIndirect',
    'vkCmdDrawIndirectCount', 'vkCmdDrawIndexedIndirectCount',
    'vkCmdDispatch', 'vkCmdDispatchIndirect', 'vkCmdDispatchBase',
    'vkCmdCopyBuffer', 'vkCmdCopyBuffer2', 'vkCmdCopyImage', 'vkCmdCopyImage2',
    'vkCmdCopyBufferToImage', 'vkCmdCopyBufferToImage2',
    'vkCmdCopyImageToBuffer', 'vkCmdCopyImageToBuffer2',
    'vkCmdBlitImage', 'vkCmdBlitImage2', 'vkCmdResolveImage', 'vkCmdResolveImage2',
    'vkCmdFillBuffer', 'vkCmdUpdateBuffer',
    'vkCmdClearColorImage', 'vkCmdClearDepthStencilImage', 'vkCmdClearAttachments',
    'vkCmdBeginRenderPass', 'vkCmdBeginRenderPass2', 'vkCmdEndRenderPass', 'vkCmdEndRenderPass2',
    'vkCmdBeginRendering', 'vkCmdEndRendering',
    'vkCmdNextSubpass', 'vkCmdNextSubpass2',
    'vkCmdBindPipeline', 'vkCmdBindVertexBuffers', 'vkCmdBindVertexBuffers2',
    'vkCmdBindIndexBuffer', 'vkCmdBindIndexBuffer2',
    'vkCmdPushConstants',
    'vkCmdSetViewport', 'vkCmdSetScissor', 'vkCmdSetDepthBias',
    'vkCmdSetLineWidth', 'vkCmdSetBlendConstants', 'vkCmdSetStencilReference',
    'vkCmdSetDepthBounds', 'vkCmdSetDepthTestEnable', 'vkCmdSetDepthWriteEnable',
    'vkCmdSetCullMode', 'vkCmdSetFrontFace', 'vkCmdSetPrimitiveTopology',
    'vkCmdBeginQuery', 'vkCmdEndQuery', 'vkCmdResetQueryPool', 'vkCmdWriteTimestamp',
    'vkCmdWriteTimestamp2',
    'vkCmdSetEvent', 'vkCmdSetEvent2', 'vkCmdResetEvent', 'vkCmdResetEvent2',
    'vkCmdWaitEvents', 'vkCmdWaitEvents2',
    'vkCmdBeginDebugUtilsLabelEXT', 'vkCmdEndDebugUtilsLabelEXT',
    'vkCmdInsertDebugUtilsLabelEXT',
    'vkAcquireNextImageKHR', 'vkAcquireNextImage2KHR',
    'vkCreateGraphicsPipelines', 'vkCreateComputePipelines',
    'vkCreateBuffer', 'vkCreateImage', 'vkCreateImageView', 'vkCreateBufferView',
    'vkCreateSampler', 'vkCreateDescriptorSetLayout', 'vkCreatePipelineLayout',
    'vkCreateRenderPass', 'vkCreateRenderPass2', 'vkCreateFramebuffer',
    'vkCreateShaderModule', 'vkCreateDescriptorPool',
    'vkAllocateDescriptorSets', 'vkAllocateCommandBuffers', 'vkAllocateMemory',
    'vkUpdateDescriptorSets',
    'vkDestroyBuffer', 'vkDestroyImage', 'vkDestroyImageView',
    'vkFreeMemory', 'vkFreeDescriptorSets', 'vkFreeCommandBuffers',
    'vkMapMemory', 'vkUnmapMemory', 'vkFlushMappedMemoryRanges',
    'vkBindBufferMemory', 'vkBindImageMemory',
    'vkBeginCommandBuffer', 'vkEndCommandBuffer',
]


def reconstruct_api_calls(data: bytes) -> list:
    """
    Attempt to reconstruct Vulkan API call sequences from the binary data.
    Uses both regex detection and known function name matching.
    """
    api_calls = []
    seen_offsets = set()

    # Method 1: Find all vk* function calls via regex
    vk_pattern = rb'vk[A-Z][A-Za-z0-9]{3,50}'
    for m in re.finditer(vk_pattern, data):
        func_name = m.group().decode('ascii')
        pos = m.start()

        # Skip if mangled or not a valid Vulkan API function signature
        if func_name not in KNOWN_VK_FUNCTIONS:
            if not re.match(r'^vk(?:Cmd|Create|Destroy|Allocate|Free|Queue|Update|Bind|Get|Acquire)[A-Z0-9][a-zA-Z0-9_]{2,}$', func_name):
                continue


        context_start = pos
        context_end = min(pos + 400, len(data))
        context = data[context_start:context_end]

        params = []
        for pm in re.finditer(rb'[\x20-\x7e]{3,80}', context):
            param_str = pm.group().decode('ascii', errors='replace').strip()
            if param_str and param_str != func_name and not param_str.startswith('vk'):
                params.append(param_str)

        api_calls.append({
            'name': func_name,
            'offset': pos,
            'parameters': params[:20],
        })
        seen_offsets.add(pos)

    # Method 2: Search for known key command fragments
    key_fragments = [
        (rb'Barrier2', 'vkCmdPipelineBarrier2'),
        (rb'DescriptorSets', 'vkCmdBindDescriptorSets'),
        (rb'DrawIndexedIndirectCount', 'vkCmdDrawIndexedIndirectCount'),
        (rb'DrawIndirectCount', 'vkCmdDrawIndirectCount'),
        (rb'FillBuffer', 'vkCmdFillBuffer'),
        (rb'Dispatch', 'vkCmdDispatch'),
        (rb'PresentKHR', 'vkQueuePresentKHR'),
        (rb'BeginRender', 'vkCmdBeginRendering'),
        (rb'EndRender', 'vkCmdEndRendering'),
    ]

    for pattern, inferred_name in key_fragments:
        for m in re.finditer(pattern, data):
            pos = m.start()
            if any(abs(pos - seen_pos) < 100 for seen_pos in seen_offsets):
                continue

            context = data[max(0, pos - 50):min(pos + 300, len(data))]
            params = []
            for pm in re.finditer(rb'[\x20-\x7e]{3,80}', context):
                param_str = pm.group().decode('ascii', errors='replace').strip()
                if param_str and not param_str.startswith('vk'):
                    params.append(param_str)

            api_calls.append({
                'name': inferred_name,
                'offset': pos,
                'parameters': params[:15],
            })
            seen_offsets.add(pos)

    api_calls.sort(key=lambda x: x['offset'])
    return api_calls


# ─── Process List Extraction ──────────────────────────────────────────────────


def extract_process_list(data: bytes) -> list:
    """Extract running process names from the trace."""
    processes = set()
    for m in re.finditer(rb'[\x20-\x7e]{2,50}\.exe', data):
        proc = m.group().decode('ascii', errors='replace').strip()
        clean = re.sub(r'^[^a-zA-Z]+', '', proc)
        if clean and len(clean) > 4:
            processes.add(clean)
    return sorted(processes)


# ─── NVTX Marker Extraction ──────────────────────────────────────────────────

def extract_nvtx_data(data: bytes) -> dict:
    """Extract NVTX domain and marker information."""
    nvtx_info = {
        'domains': [],
        'markers': [],
        'ranges': [],
    }
    for m in re.finditer(rb'(?:Default )?NVTX Domain[\x00-\xff]{0,5}([\x20-\x7e]{3,50})', data):
        domain = m.group(0).decode('ascii', errors='replace')
        nvtx_info['domains'].append(domain)

    for m in re.finditer(rb'(\d+) Frames', data):
        nvtx_info['markers'].append(m.group().decode('ascii'))

    for m in re.finditer(rb'ApplicationFrame[\x20-\x7e]*', data):
        nvtx_info['ranges'].append(m.group().decode('ascii', errors='replace'))

    return nvtx_info


# ─── Output Generation ────────────────────────────────────────────────────────

def generate_output(extracted: ExtractedData, verbosity: str, input_path: str) -> str:
    """Generate the comprehensive AI-readable text output."""
    lines = []
    w = lines.append

    w("=" * 100)
    w("NVIDIA NSIGHT GRAPHICS GPU TRACE — COMPREHENSIVE AI-READABLE EXTRACTION")
    w("=" * 100)
    w("")
    w(f"Source File:     {os.path.basename(input_path)}")
    w(f"File Size:       {extracted.header.file_size:,} bytes ({extracted.header.file_size / 1048576:.1f} MB)")
    w(f"File Magic:      {extracted.header.magic}")
    w(f"Extraction Date: Generated by nsight_trace_to_text.py")
    w(f"Verbosity:       {verbosity}")
    w("")

    # ── Section 1: System & Hardware Information ──
    w("=" * 100)
    w("SECTION 1: SYSTEM & HARDWARE INFORMATION")
    w("=" * 100)
    w("")
    si = extracted.system_info
    rows = [
        ("Computer Name", si.computer_name),
        ("Operating System", si.operating_system),
        ("OS Build", si.os_build),
        ("Processor (CPU)", si.processor),
        ("RAM Usage", si.ram_usage),
        ("GPU Device", si.gpu_device),
        ("GPU Chip", si.gpu_chip),
        ("GPU Driver Version", si.driver_version),
        ("Hardware Scheduling", si.hardware_scheduling),
        ("VRAM Requested", si.vram_requested),
        ("VRAM Committed", si.vram_committed),
        ("Bytes Demoted", si.bytes_demoted),
    ]
    for label, val in rows:
        if val:
            w(f"  {label:<30s} {val}")
    w("")

    # ── Section 2: Capture & Profiling Configuration ──
    w("=" * 100)
    w("SECTION 2: CAPTURE & PROFILING CONFIGURATION")
    w("=" * 100)
    w("")
    cs = extracted.capture_settings
    pm = extracted.profiling_metadata
    rows = [
        ("Nsight Graphics Version", cs.product_version),
        ("Graphics API", cs.graphics_api),
        ("Executable Path", cs.executable_path),
        ("Command Line", cs.command_line),
        ("Start After", cs.start_after),
        ("Max Duration", cs.max_duration),
        ("Frame Limit", cs.limited_to),
        ("Event Memory", cs.allocated_event_memory),
        ("V-Sync Mode", cs.vsync_mode),
        ("GPU Clocks", cs.gpu_clocks),
        ("Screenshot", cs.screenshot_enabled),
        ("Time Every Action", cs.time_every_action),
        ("Shader Bindings", cs.trace_shader_bindings),
        ("Pipeline Collection", cs.pipeline_collection),
        ("Debug Info", cs.debug_info),
        ("NVTX Ranges", cs.nvtx_ranges),
        ("Recompile Cached Shaders", cs.recompile_cached),
    ]
    for label, val in rows:
        if val:
            w(f"  {label:<30s} {val}")
    w("")

    # ── Section 3: Profiling Metadata ──
    w("=" * 100)
    w("SECTION 3: GPU PROFILING METADATA")
    w("=" * 100)
    w("")
    rows = [
        ("Warp Sample Duration", pm.sample_duration),
        ("Warp State Sampling", pm.warp_state_info),
        ("Sampling Interval", pm.sampling_interval),
        ("PMA Buffer Size", pm.pma_buffer_size),
    ]
    for label, val in rows:
        if val:
            w(f"  {label:<30s} {val}")
    w("")
    w("  Note: The SM warp sampling profiler collects warp state distributions at the")
    w("  above interval. Each sample captures what state all active warps are in (e.g.,")
    w("  active, stalled on memory, stalled on barrier, etc.). This data is stored as")
    w("  packed binary counters in the trace and requires Nsight Graphics GUI for full")
    w("  visualization. Key performance insights from warp states:")
    w("    - High 'Stall: Data Request' → Memory bandwidth bottleneck")
    w("    - High 'Stall: Texture Fetch' → Texture cache misses / divergent sampling")
    w("    - High 'Stall: Barrier' → Excessive synchronization")
    w("    - High 'Stall: Not Selected' → Low occupancy / register pressure")
    w("")

    # ── Section 3.5: Layout File Hardware Metrics ──
    if extracted.layout_info:
        w("=" * 100)
        w("SECTION 3.5: CAPTURED GPU HARDWARE METRICS & TIMELINE TRACKS (from .layout)")
        w("=" * 100)
        w("")
        w(f"  Layout File:            {extracted.layout_info.get('layout_file')}")
        vr = extracted.layout_info.get('visible_range', (None, None))
        if vr and vr[0] is not None:
            w(f"  Visible Timeline Range: {vr[0]:,} to {vr[1]:,} ns/cycles")
        exp = extracted.layout_info.get('expanded_events', [])
        if exp:
            w(f"  Expanded Event Indices: {', '.join(str(i) for i in exp[:20])}")
        metrics = extracted.layout_info.get('captured_metrics', [])
        if metrics:
            w(f"  Captured Hardware Metrics & Timeline Tracks ({len(metrics)} tracks):")
            for m in metrics:
                w(f"    - {m}")
        w("")

    # ── Section 4: Vulkan Swapchain & Presentation ──
    w("=" * 100)
    w("SECTION 4: VULKAN SWAPCHAIN & PRESENTATION")
    w("=" * 100)
    w("")

    cats = extracted.all_categorized_strings
    formats = cats.get('format_info', [])
    presents = cats.get('present_modes', [])
    swapchain = cats.get('swapchain_info', [])

    if formats:
        w("  Surface/Image Formats Found:")
        for _, s in formats:
            w(f"    - {s}")
    if presents:
        w("  Present Modes Found:")
        for _, s in presents:
            w(f"    - {s}")
    if swapchain:
        w("  Swapchain Info:")
        for _, s in swapchain:
            w(f"    - {s}")
    w("")

    # ── Section 5: Vulkan API Call Stream ──
    w("=" * 100)
    w("SECTION 5: VULKAN API CALL STREAM (RECONSTRUCTED)")
    w("=" * 100)
    w("")
    w("  Note: API calls are extracted from the binary trace. Parameter values may be")
    w("  partial due to the binary encoding. Hex values represent GPU resource handles.")
    w("")

    api_calls = extracted.vulkan_api_calls
    if api_calls:
        call_counts = defaultdict(int)
        for call in api_calls:
            call_counts[call['name']] += 1

        w("  API Call Summary:")
        w(f"  {'Function':<45s} {'Count':>6s}")
        w(f"  {'-'*45} {'-'*6}")
        for func, count in sorted(call_counts.items(), key=lambda x: -x[1]):
            w(f"  {func:<45s} {count:>6d}")
        w("")

        if verbosity in ('normal', 'verbose'):
            w("  Detailed API Calls (in trace order):")
            w("")
            for i, call in enumerate(api_calls):
                w(f"  [{i+1:4d}] {call['name']} (offset: 0x{call['offset']:08x})")
                if verbosity == 'verbose' and call['parameters']:
                    for p in call['parameters'][:15]:
                        w(f"         | {p}")
                w("")
    else:
        w("  No API calls found in trace.")
    w("")

    # ── Section 6: Pipeline State ──
    w("=" * 100)
    w("SECTION 6: PIPELINE STATE & SHADER BINDINGS")
    w("=" * 100)
    w("")

    stages = cats.get('pipeline_stages', [])
    if stages:
        w("  Pipeline Stages Referenced:")
        seen_stages = set()
        for _, s in stages:
            if s not in seen_stages:
                w(f"    - {s}")
                seen_stages.add(s)

    shaders = cats.get('shader_info', [])
    if shaders:
        w("")
        w("  Shader Stage References:")
        seen_shaders = set()
        for _, s in shaders:
            if s not in seen_shaders:
                w(f"    - {s}")
                seen_shaders.add(s)

    descriptors = cats.get('descriptor_info', [])
    if descriptors:
        w("")
        w("  Descriptor Set / Binding Info:")
        seen_desc = set()
        for _, s in descriptors:
            if s not in seen_desc:
                w(f"    - {s}")
                seen_desc.add(s)
    w("")

    # ── Section 7: Memory Barriers & Synchronization ──
    w("=" * 100)
    w("SECTION 7: MEMORY BARRIERS & SYNCHRONIZATION")
    w("=" * 100)
    w("")

    access = cats.get('access_flags', [])
    layouts = cats.get('image_layouts', [])
    barriers = cats.get('barriers', [])
    syncs = cats.get('synchronization', [])

    if barriers:
        w("  Barrier Commands:")
        seen = set()
        for _, s in barriers:
            if s not in seen:
                w(f"    - {s}")
                seen.add(s)

    if access:
        w("")
        w("  Access Flags Referenced:")
        seen = set()
        for _, s in access:
            if s not in seen:
                w(f"    - {s}")
                seen.add(s)

    if layouts:
        w("")
        w("  Image Layouts Referenced:")
        seen = set()
        for _, s in layouts:
            if s not in seen:
                w(f"    - {s}")
                seen.add(s)

    if syncs:
        w("")
        w("  Synchronization Primitives:")
        seen = set()
        for _, s in syncs:
            if s not in seen:
                w(f"    - {s}")
                seen.add(s)
    w("")

    # ── Section 8: Draw Calls & Dispatches ──
    w("=" * 100)
    w("SECTION 8: DRAW CALLS & COMPUTE DISPATCHES")
    w("=" * 100)
    w("")

    draws = cats.get('draw_calls', [])
    dispatches = cats.get('dispatches', [])

    if draws:
        w("  Draw Call Related Strings:")
        seen = set()
        for _, s in draws:
            if s not in seen:
                w(f"    - {s}")
                seen.add(s)

    if dispatches:
        w("")
        w("  Compute Dispatch Related Strings:")
        seen = set()
        for _, s in dispatches:
            if s not in seen:
                w(f"    - {s}")
                seen.add(s)

    mem = cats.get('memory_buffer_info', [])
    if mem and verbosity in ('normal', 'verbose'):
        w("")
        w("  Memory/Buffer Parameters:")
        seen = set()
        for _, s in mem:
            if s not in seen:
                w(f"    - {s}")
                seen.add(s)
    w("")

    # ── Section 9: NVTX Annotations ──
    w("=" * 100)
    w("SECTION 9: NVTX ANNOTATIONS & MARKERS")
    w("=" * 100)
    w("")

    nvtx = cats.get('nvtx_markers', [])
    if nvtx:
        w("  NVTX Markers Found:")
        seen = set()
        for _, s in nvtx:
            if s not in seen:
                w(f"    - {s}")
                seen.add(s)
    else:
        w("  No NVTX markers found (or application did not use NVTX API).")
    w("")

    # ── Section 10: Running Processes ──
    w("=" * 100)
    w("SECTION 10: RUNNING PROCESSES (at capture time)")
    w("=" * 100)
    w("")

    procs = cats.get('processes', [])
    if procs:
        seen = set()
        for _, s in procs:
            if s not in seen:
                w(f"    - {s}")
                seen.add(s)
    else:
        w("  No process list extracted.")
    w("")

    # ── Section 11: File Paths ──
    w("=" * 100)
    w("SECTION 11: FILE PATHS & CONFIGURATION")
    w("=" * 100)
    w("")

    paths = cats.get('file_paths', [])
    if paths:
        seen = set()
        for _, s in paths:
            if s not in seen and len(s) > 6:
                w(f"    {s}")
                seen.add(s)
    w("")

    # ── Section 12: Named Constants & Enums ──
    w("=" * 100)
    w("SECTION 12: VULKAN ENUMS & NAMED CONSTANTS")
    w("=" * 100)
    w("")

    enums = cats.get('vulkan_enums', [])
    constants = cats.get('named_constants', [])

    if enums:
        w("  Vulkan Type/Enum References:")
        seen = set()
        for _, s in enums:
            if s not in seen:
                w(f"    - {s}")
                seen.add(s)

    if constants and verbosity in ('normal', 'verbose'):
        w("")
        w("  Other Named Constants:")
        seen = set()
        for _, s in constants:
            if s not in seen:
                w(f"    - {s}")
                seen.add(s)
    w("")

    # ── Final Summary Statistics ──
    w("=" * 100)
    w("EXTRACTION SUMMARY & STATISTICS")
    w("=" * 100)
    w("")
    total_strings = sum(len(v) for v in cats.values())
    w(f"  Total unique strings extracted:       {total_strings:,}")
    w(f"  API calls reconstructed:             {len(api_calls):,}")
    w("")
    w("  Strings by category:")
    for cat, items in sorted(cats.items(), key=lambda x: -len(x[1])):
        w(f"    {cat:<35s} {len(items):>6d}")
    w("")

    w("=" * 100)
    w("END OF EXTRACTION")
    w("=" * 100)

    return "\n".join(lines)


# ─── Diff Comparison ───────────────────────────────────────────────────────────



def generate_diff_output(file1: str, file2: str) -> str:
    """Compare two Nsight trace analysis files side-by-side."""
    lines = []
    w = lines.append
    w("=" * 100)
    w("NSIGHT GPU TRACE DIFF COMPARISON SUMMARY")
    w("=" * 100)
    w(f"  File A: {os.path.basename(file1)}")
    w(f"  File B: {os.path.basename(file2)}")
    w("")

    def load_counts(path):
        counts = defaultdict(int)
        if not os.path.exists(path):
            return counts
        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            in_table = False
            for line in f:
                if 'API Call Summary:' in line:
                    in_table = True
                    continue
                if in_table and '=====' in line:
                    break
                if in_table and line.strip() and not line.startswith('Function'):
                    parts = line.split()
                    if len(parts) >= 2 and parts[-1].isdigit():
                        func = parts[0]
                        counts[func] = int(parts[-1])
        return counts

    c1 = load_counts(file1)
    c2 = load_counts(file2)
    all_funcs = sorted(set(c1.keys()) | set(c2.keys()))

    w(f"  {'Function':<50s} {'File A':>10s} {'File B':>10s} {'Diff':>10s}")
    w(f"  {'-'*50} {'-'*10} {'-'*10} {'-'*10}")
    for func in all_funcs:
        v1 = c1[func]
        v2 = c2[func]
        diff = v2 - v1
        diff_str = f"{diff:+d}" if diff != 0 else "0"
        w(f"  {func:<50s} {v1:>10d} {v2:>10d} {diff_str:>10s}")

    w("")
    w("=" * 100)
    w("END OF DIFF COMPARISON")
    w("=" * 100)
    return "\n".join(lines)


# ─── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Convert NVIDIA Nsight Graphics GPU Trace (.ngfx-gputrace) to AI-readable text.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python nsight_trace_to_text.py trace.ngfx-gputrace
  python nsight_trace_to_text.py trace.ngfx-gputrace --output analysis.txt
  python nsight_trace_to_text.py trace.ngfx-gputrace --diff other_analysis.txt
  python nsight_trace_to_text.py trace.ngfx-gputrace --verbosity verbose
        """
    )
    parser.add_argument("input", help="Path to .ngfx-gputrace file or previous analysis text file")
    parser.add_argument("--output", "-o", help="Output text file path (default: <input>_analysis.txt)")
    parser.add_argument("--verbosity", "-v", choices=["summary", "normal", "verbose"],
                        default="normal", help="Output verbosity level (default: normal)")
    parser.add_argument("--diff", help="Path to second trace analysis file to diff against input")

    args = parser.parse_args()

    input_path = Path(args.input)
    if not input_path.exists():
        print(f"ERROR: File not found: {input_path}", file=sys.stderr)
        sys.exit(1)

    if args.diff:
        diff_path = Path(args.diff)
        if not diff_path.exists():
            print(f"ERROR: Diff target not found: {diff_path}", file=sys.stderr)
            sys.exit(1)
        output_text = generate_diff_output(str(input_path), str(diff_path))
        output_path = Path(args.output) if args.output else input_path.with_name(input_path.stem + "_diff.txt")
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(output_text)
        print(f"Diff output written to {output_path}")
        return

    # Default output path
    if args.output:
        output_path = Path(args.output)
    else:
        output_path = input_path.with_name(input_path.stem + "_analysis.txt")

    print(f"Reading {input_path} ({input_path.stat().st_size / 1048576:.1f} MB)...")

    with open(input_path, 'rb') as f:
        data = f.read()

    print(f"Parsing header...")
    extracted = ExtractedData()
    extracted.header = parse_header(data)

    print(f"Extracting metadata (system info, capture settings)...")
    extracted.system_info, extracted.capture_settings, extracted.profiling_metadata = \
        parse_metadata_section(data)

    print(f"Extracting and categorizing all strings...")
    extracted.all_categorized_strings = extract_and_categorize_strings(data)

    print(f"Reconstructing API call stream...")
    extracted.vulkan_api_calls = reconstruct_api_calls(data)

    print(f"Extracting NVTX data...")
    nvtx_data = extract_nvtx_data(data)

    layout_file = input_path.with_name(input_path.name + ".layout")
    if not layout_file.exists():
        layout_file = input_path.with_suffix('.layout')
    if layout_file.exists():
        print(f"Parsing layout file ({layout_file.name})...")
        extracted.layout_info = parse_layout_file(layout_file)

    print(f"Generating output ({args.verbosity} verbosity)...")

    output_text = generate_output(extracted, args.verbosity, str(input_path))

    print(f"Writing to {output_path}...")
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(output_text)

    line_count = output_text.count('\n')
    print(f"Done! Output: {output_path} ({os.path.getsize(output_path):,} bytes, {line_count:,} lines)")


if __name__ == "__main__":
    main()

