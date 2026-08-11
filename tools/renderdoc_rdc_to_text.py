#!/usr/bin/env python3
"""
renderdoc_rdc_to_text.py — Convert RenderDoc Capture (.rdc) into a comprehensive,
                           AI-readable text file.

Uses `renderdoccmd.exe` (bundled with RenderDoc installation) to convert the
binary RDC into XML format, then parses the XML with streaming parser to extract
every piece of information: API calls, resource creation, pipeline state, render
pass structure, synchronization, memory allocations, and more.

Usage:
    python renderdoc_rdc_to_text.py <input.rdc> [--output output.txt] [--verbosity summary|normal|verbose]
    python renderdoc_rdc_to_text.py <input.rdc> --renderdoccmd "C:\\Path\\To\\renderdoccmd.exe"

Requires: Python 3.8+ (no external pip dependencies)
          RenderDoc installed (renderdoccmd.exe must be accessible)
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
from collections import defaultdict, OrderedDict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


# ─── Constants ─────────────────────────────────────────────────────────────────

DEFAULT_RENDERDOCCMD_PATHS = [
    r"C:\Program Files\RenderDoc\renderdoccmd.exe",
    r"C:\Program Files (x86)\RenderDoc\renderdoccmd.exe",
    # Linux/macOS
    "/usr/bin/renderdoccmd",
    "/usr/local/bin/renderdoccmd",
    "/opt/renderdoc/bin/renderdoccmd",
]

# Known embedded sections in RDC files
KNOWN_SECTIONS = [
    "renderdoc/internal/resolvedb",
    "renderdoc/internal/driver",
    "renderdoc/internal/core",
    "thumbnail",
]

# Vulkan API call categories
VK_CALL_CATEGORIES = {
    'creation': re.compile(r'vkCreate|vkAllocate'),
    'destruction': re.compile(r'vkDestroy|vkFree'),
    'binding': re.compile(r'vkBind|vkCmd.*Bind'),
    'draw': re.compile(r'vkCmdDraw'),
    'dispatch': re.compile(r'vkCmdDispatch'),
    'copy': re.compile(r'vkCmdCopy|vkCmdBlit|vkCmdResolve'),
    'clear': re.compile(r'vkCmdClear|vkCmdFill'),
    'sync': re.compile(r'vkCmdPipelineBarrier|vkCmdWait|vkCmdSet.*Event|vkCmd.*Semaphore'),
    'renderpass': re.compile(r'vkCmdBeginRender|vkCmdEndRender|vkCmdNextSubpass'),
    'query': re.compile(r'vkCmdBeginQuery|vkCmdEndQuery|vkCmdWriteTimestamp'),
    'push_constants': re.compile(r'vkCmdPushConstants'),
    'descriptor': re.compile(r'vkCmdBindDescriptor|vkUpdateDescriptor|vkCmdPushDescriptor'),
    'state': re.compile(r'vkCmdSet(?!Event)'),
    'submit': re.compile(r'vkQueueSubmit|vkQueuePresent'),
    'transfer': re.compile(r'vkCmdCopyBuffer|vkCmdCopyImage|vkCmdUpdateBuffer'),
    'debug': re.compile(r'vkCmdDebug|vkCmdInsert|vkCmdBeginDebug|vkCmdEndDebug'),
}


# ─── Data Classes ──────────────────────────────────────────────────────────────

@dataclass
class RDCMetadata:
    file_path: str = ""
    file_size: int = 0
    api: str = ""
    driver_info: str = ""
    machine_info: str = ""
    capture_date: str = ""
    renderdoc_version: str = ""


@dataclass
class FrameStats:
    total_api_calls: int = 0
    total_draw_calls: int = 0
    total_dispatches: int = 0
    total_barriers: int = 0
    total_render_passes: int = 0
    total_resources_created: int = 0
    total_buffers: int = 0
    total_images: int = 0
    total_descriptor_updates: int = 0
    total_push_constant_calls: int = 0
    total_copy_calls: int = 0
    total_clear_calls: int = 0
    total_query_calls: int = 0
    total_submit_calls: int = 0


@dataclass
class APICall:
    name: str = ""
    event_id: int = 0
    parameters: dict = field(default_factory=dict)
    children: list = field(default_factory=list)
    category: str = "other"
    duration_us: float = 0.0
    timestamp: int = 0
    thread_id: str = ""
    pipeline_state: dict = field(default_factory=dict)



@dataclass
class ResourceInfo:
    resource_type: str = ""
    name: str = ""
    handle: str = ""
    details: dict = field(default_factory=dict)


# ─── Find renderdoccmd ─────────────────────────────────────────────────────────

def find_renderdoccmd(custom_path: Optional[str] = None) -> str:
    """Locate the renderdoccmd executable."""
    if custom_path:
        if os.path.isfile(custom_path):
            return custom_path
        print(f"ERROR: Specified renderdoccmd not found: {custom_path}", file=sys.stderr)
        sys.exit(1)

    # Check PATH first
    found = shutil.which("renderdoccmd")
    if found:
        return found

    # Check default locations
    for path in DEFAULT_RENDERDOCCMD_PATHS:
        if os.path.isfile(path):
            return path

    print("ERROR: renderdoccmd.exe not found.", file=sys.stderr)
    print("Please install RenderDoc or specify path with --renderdoccmd", file=sys.stderr)
    sys.exit(1)


# ─── XML Conversion via renderdoccmd ───────────────────────────────────────────

def convert_rdc_to_xml(rdc_path: str, renderdoccmd: str, output_dir: str) -> str:
    """Convert RDC capture file to XML (or ZIP containing XML and shader chunks)."""
    xml_path = os.path.join(output_dir, "capture.xml")
    zip_path = os.path.join(output_dir, "capture.zip")

    print(f"  Converting RDC to XML/ZIP via renderdoccmd...")
    print(f"  This may take several minutes for large captures...")

    # Try zip.xml first so binary chunk files (including SPIR-V shaders) are extracted
    cmd_zip = [renderdoccmd, "convert", "-f", rdc_path, "-o", zip_path, "-c", "zip.xml"]
    try:
        res = subprocess.run(cmd_zip, capture_output=True, text=True, timeout=3600)
        if os.path.exists(zip_path) and os.path.getsize(zip_path) > 0:
            import zipfile
            with zipfile.ZipFile(zip_path, 'r') as zf:
                zf.extractall(output_dir)
            if os.path.exists(xml_path):
                print(f"  ZIP.XML exported & uncompressed: {xml_path} ({os.path.getsize(xml_path) / 1048576:.1f} MB)")
                return xml_path
    except Exception:
        pass

    # Fallback to plain XML conversion
    cmd = [
        renderdoccmd, "convert",
        "-f", rdc_path,
        "-o", xml_path,
        "-c", "xml",
    ]

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=3600,  # 1 hour timeout for huge captures
        )

        if result.returncode != 0:
            stderr = result.stderr.strip()
            # renderdoccmd writes usage to stderr even on success sometimes
            if not os.path.exists(xml_path):
                print(f"  WARNING: renderdoccmd returned code {result.returncode}", file=sys.stderr)
                if stderr:
                    print(f"  stderr: {stderr}", file=sys.stderr)
                return ""

        if os.path.exists(xml_path):
            size = os.path.getsize(xml_path)
            print(f"  XML generated: {xml_path} ({size / 1048576:.1f} MB)")
            return xml_path
        else:
            print(f"  ERROR: XML file was not created.", file=sys.stderr)
            return ""

    except subprocess.TimeoutExpired:
        print(f"  ERROR: Conversion timed out after 1 hour.", file=sys.stderr)
        return ""
    except FileNotFoundError:
        print(f"  ERROR: renderdoccmd not found at: {renderdoccmd}", file=sys.stderr)
        return ""


def convert_rdc_to_chrome_json(rdc_path: str, renderdoccmd: str, output_dir: str) -> str:
    """Also export chrome profiler JSON for timing/event data."""
    json_path = os.path.join(output_dir, "capture_chrome.json")

    cmd = [
        renderdoccmd, "convert",
        "-f", rdc_path,
        "-o", json_path,
        "-c", "chrome.json",
    ]

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=3600)
        if os.path.exists(json_path) and os.path.getsize(json_path) > 0:
            print(f"  Chrome JSON generated: {json_path} ({os.path.getsize(json_path) / 1048576:.1f} MB)")
            return json_path
    except (subprocess.TimeoutExpired, FileNotFoundError):
        pass

    return ""


# ─── Extract Embedded Sections ─────────────────────────────────────────────────

def extract_sections(rdc_path: str, renderdoccmd: str, output_dir: str) -> dict:
    """Try to extract known embedded sections from the RDC."""
    sections = {}

    for section_name in KNOWN_SECTIONS:
        safe_name = section_name.replace("/", "_").replace("\\", "_")
        section_file = os.path.join(output_dir, f"section_{safe_name}.bin")

        cmd = [
            renderdoccmd, "extract",
            "--section", section_name,
            "--file", section_file,
            rdc_path,
        ]

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
            if os.path.exists(section_file) and os.path.getsize(section_file) > 0:
                with open(section_file, 'rb') as f:
                    raw = f.read()
                sections[section_name] = raw
                print(f"  Extracted section '{section_name}': {len(raw):,} bytes")
        except (subprocess.TimeoutExpired, FileNotFoundError):
            pass

    return sections


def extract_thumbnail(rdc_path: str, renderdoccmd: str, output_dir: str) -> str:
    """Extract the embedded thumbnail image."""
    thumb_path = os.path.join(output_dir, "thumbnail.jpg")

    cmd = [renderdoccmd, "thumb", "-o", thumb_path, rdc_path]

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        if os.path.exists(thumb_path) and os.path.getsize(thumb_path) > 0:
            print(f"  Thumbnail extracted: {thumb_path} ({os.path.getsize(thumb_path):,} bytes)")
            return thumb_path
    except (subprocess.TimeoutExpired, FileNotFoundError):
        pass

    return ""


# ─── XML Parsing ──────────────────────────────────────────────────────────────

def extract_chunk_params(elem, max_depth: int = 3, current_depth: int = 0) -> dict:
    """Recursively extract key-value parameter structure from a chunk XML node."""
    if current_depth > max_depth:
        return {}
    params = {}
    for child in elem:
        name = child.get('name', '') or child.tag
        val_str = child.get('string', '')
        if not val_str:
            if child.text and child.text.strip():
                val_str = child.text.strip()
            elif 'id' in child.attrib:
                val_str = child.get('id', '')
        if val_str and val_str not in ('0', 'false', '0.000000'):
            params[name] = val_str
        if len(child) > 0 and current_depth < max_depth:
            sub = extract_chunk_params(child, max_depth, current_depth + 1)
            for k, v in sub.items():
                params[f"{name}.{k}"] = v
    return params


def parse_xml_streaming(xml_path: str, verbosity: str) -> tuple:
    """
    Parse RenderDoc XML export using streaming (iterparse). Handles modern <chunk>
    elements, <header> metadata, timestamps, and parameters.
    """
    api_calls = []
    resources = []
    stats = FrameStats()
    metadata = RDCMetadata()

    max_calls = {
        'summary': 0,
        'normal': 50000,
        'verbose': 500000,
    }.get(verbosity, 50000)

    timebase_freq = 1.0

    print(f"  Parsing XML ({verbosity} mode)...")

    try:
        context = ET.iterparse(xml_path, events=('end',))
        event_id_counter = 0

        for _, elem in context:
            tag = elem.tag.lower() if elem.tag else ""

            if tag == 'driver':
                metadata.api = elem.text or elem.get('name', 'Vulkan')
            elif tag == 'machineident':
                metadata.machine_info = f"Machine ID: {elem.text}"
            elif tag == 'timebase':
                freq_str = elem.get('frequency', '10')
                try:
                    timebase_freq = float(freq_str) if float(freq_str) > 0 else 1.0
                except ValueError:
                    timebase_freq = 1.0

            elif tag in ('chunk', 'call', 'apicall', 'action', 'event', 'drawcall'):
                call_name = elem.get('name', '') or elem.get('Name', '') or elem.text or ""
                event_id_counter += 1

                if call_name:
                    stats.total_api_calls += 1

                    category = 'other'
                    for cat_name, pattern in VK_CALL_CATEGORIES.items():
                        if pattern.search(call_name):
                            category = cat_name
                            break

                    if category == 'draw':
                        stats.total_draw_calls += 1
                    elif category == 'dispatch':
                        stats.total_dispatches += 1
                    elif category == 'sync':
                        stats.total_barriers += 1
                    elif category == 'renderpass':
                        stats.total_render_passes += 1
                    elif category == 'creation':
                        stats.total_resources_created += 1
                    elif category == 'descriptor':
                        stats.total_descriptor_updates += 1
                    elif category == 'push_constants':
                        stats.total_push_constant_calls += 1
                    elif category in ('copy', 'transfer'):
                        stats.total_copy_calls += 1
                    elif category == 'clear':
                        stats.total_clear_calls += 1
                    elif category == 'query':
                        stats.total_query_calls += 1
                    elif category == 'submit':
                        stats.total_submit_calls += 1

                    params = extract_chunk_params(elem)

                    if len(api_calls) < max_calls:
                        ts = int(elem.get('timestamp', '0'))
                        dur = int(elem.get('duration', '0'))
                        thread_id = elem.get('threadID', '')
                        dur_us = (dur / timebase_freq) if timebase_freq > 0 else float(dur)

                        call = APICall(
                            name=call_name,
                            event_id=event_id_counter,
                            category=category,
                            timestamp=ts,
                            duration_us=dur_us,
                            thread_id=thread_id,
                            parameters=params
                        )
                        api_calls.append(call)

                    # Track resource creations
                    if category == 'creation' or 'Create' in call_name or 'Allocate' in call_name:
                        res_handle = ""
                        res_name = ""
                        res_details = {}
                        for k, v in params.items():
                            kl = k.lower()
                            if ('pcreateinfo.name' in kl or 'debuglabel' in kl) and v and not res_name:
                                res_name = v
                            if not res_handle:
                                if any(h in kl for h in ('handle', 'resourceid', 'image', 'buffer', 'memory', 'view', 'sampler', 'pipeline', 'set', 'pool', 'swapchain', 'fence', 'semaphore', 'shadermodule')):
                                    if v.isdigit() or v.startswith('ResourceId') or v.startswith('0x'):
                                        res_handle = f"ResourceId::{v}" if v.isdigit() else v
                            if any(d in kl for d in ('format', 'extent', 'size', 'stage', 'usage', 'type', 'width', 'height')):
                                res_details[k.split('.')[-1]] = v

                        if not res_name:
                            desc = []
                            if 'CreateInfo.format' in params: desc.append(params['CreateInfo.format'])
                            if 'CreateInfo.extent.width' in params and 'CreateInfo.extent.height' in params:
                                desc.append(f"{params['CreateInfo.extent.width']}x{params['CreateInfo.extent.height']}")
                            if 'CreateInfo.size' in params:
                                try: desc.append(f"{int(params['CreateInfo.size']):,}B")
                                except ValueError: desc.append(f"{params['CreateInfo.size']}B")
                            if 'memoryRequirements.size' in params:
                                try: desc.append(f"mem:{int(params['memoryRequirements.size']):,}B")
                                except ValueError: desc.append(f"mem:{params['memoryRequirements.size']}B")
                            res_name = " ".join(desc)

                        resources.append(ResourceInfo(
                            resource_type=call_name.replace('vkCreate', '').replace('vkAllocate', ''),
                            name=res_name,
                            handle=res_handle,
                            details=res_details if res_details else params
                        ))

                elem.clear()


    except ET.ParseError as e:
        print(f"  WARNING: XML parse error: {e}", file=sys.stderr)
    except Exception as e:
        print(f"  WARNING: Error during XML parsing: {e}", file=sys.stderr)

    if stats.total_api_calls == 0:
        print(f"  XML had no recognized API call elements. Attempting flat text scan...")
        stats, api_calls, resources = parse_xml_flat(xml_path, verbosity, max_calls)

    return api_calls, resources, stats, metadata



def parse_xml_flat(xml_path: str, verbosity: str, max_calls: int) -> tuple:
    """
    Fallback: scan the XML file line-by-line for API call patterns.
    RenderDoc XML format may vary between versions.
    """
    stats = FrameStats()
    api_calls = []
    resources = []
    call_counter = defaultdict(int)

    vk_call_pattern = re.compile(r'(vk[A-Z][A-Za-z0-9]+)')
    param_pattern = re.compile(r'(\w+)\s*=\s*"?([^"<>\n]+)"?')
    resource_pattern = re.compile(r'<(Buffer|Image|Texture|Sampler|Pipeline|Shader|DescriptorSet|RenderPass|Framebuffer)\b[^>]*>', re.IGNORECASE)

    event_id = 0

    try:
        with open(xml_path, 'r', encoding='utf-8', errors='replace') as f:
            for line in f:
                # Find API calls
                for m in vk_call_pattern.finditer(line):
                    call_name = m.group(1)
                    event_id += 1
                    stats.total_api_calls += 1
                    call_counter[call_name] += 1

                    category = 'other'
                    for cat_name, pattern in VK_CALL_CATEGORIES.items():
                        if pattern.search(call_name):
                            category = cat_name
                            break

                    if category == 'draw':
                        stats.total_draw_calls += 1
                    elif category == 'dispatch':
                        stats.total_dispatches += 1
                    elif category == 'sync':
                        stats.total_barriers += 1
                    elif category == 'renderpass':
                        stats.total_render_passes += 1
                    elif category == 'creation':
                        stats.total_resources_created += 1
                    elif category == 'submit':
                        stats.total_submit_calls += 1

                    if len(api_calls) < max_calls:
                        call = APICall(name=call_name, event_id=event_id, category=category)
                        # Extract inline parameters
                        for pm in param_pattern.finditer(line):
                            call.parameters[pm.group(1)] = pm.group(2)
                        api_calls.append(call)

                # Find resources
                for m in resource_pattern.finditer(line):
                    res_type = m.group(1)
                    details = dict(param_pattern.findall(line))
                    resources.append(ResourceInfo(
                        resource_type=res_type,
                        name=details.get('name', ''),
                        handle=details.get('handle', details.get('id', '')),
                        details=details,
                    ))

    except Exception as e:
        print(f"  WARNING: Flat parsing error: {e}", file=sys.stderr)

    return stats, api_calls, resources


# ─── Chrome JSON Parsing ──────────────────────────────────────────────────────

def parse_chrome_json(json_path: str) -> dict:
    """Parse Chrome profiler JSON for timing/event hierarchy data."""
    result = {
        'events': [],
        'total_duration_us': 0,
        'categories': defaultdict(int),
    }

    if not json_path or not os.path.exists(json_path):
        return result

    try:
        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)

        events = data if isinstance(data, list) else data.get('traceEvents', [])

        for ev in events:
            name = ev.get('name', '')
            cat = ev.get('cat', '')
            dur = ev.get('dur', 0)

            result['categories'][cat] += 1
            result['total_duration_us'] += dur

            if name:
                result['events'].append({
                    'name': name,
                    'category': cat,
                    'duration_us': dur,
                    'timestamp_us': ev.get('ts', 0),
                    'phase': ev.get('ph', ''),
                    'args': ev.get('args', {}),
                })

    except (json.JSONDecodeError, Exception) as e:
        print(f"  WARNING: Chrome JSON parse error: {e}", file=sys.stderr)

    return result


# ─── Section Data Parsing ─────────────────────────────────────────────────────

def parse_section_strings(sections: dict) -> dict:
    """Extract readable strings from binary sections."""
    result = {}

    for name, data in sections.items():
        strings = []
        for m in re.finditer(rb'[\x20-\x7e]{4,200}', data):
            s = m.group().decode('ascii', errors='replace').strip()
            if s:
                strings.append(s)
        result[name] = strings

    return result


# ─── Output Generation ────────────────────────────────────────────────────────

def generate_output(
    rdc_path: str,
    metadata: RDCMetadata,
    stats: FrameStats,
    api_calls: list,
    resources: list,
    chrome_data: dict,
    section_strings: dict,
    thumbnail_path: str,
    verbosity: str,
) -> str:
    """Generate the comprehensive AI-readable text output."""
    lines = []
    w = lines.append

    w("=" * 100)
    w("RENDERDOC CAPTURE (.RDC) — COMPREHENSIVE AI-READABLE EXTRACTION")
    w("=" * 100)
    w("")
    w(f"Source File:      {os.path.basename(rdc_path)}")
    w(f"File Size:        {os.path.getsize(rdc_path):,} bytes ({os.path.getsize(rdc_path) / 1073741824:.2f} GB)")
    w(f"Extraction Date:  Generated by renderdoc_rdc_to_text.py")
    w(f"Verbosity:        {verbosity}")
    if thumbnail_path:
        w(f"Thumbnail:        Extracted (see accompanying .jpg file)")
    w("")

    # ── Section 1: Capture Metadata ──
    w("=" * 100)
    w("SECTION 1: CAPTURE METADATA")
    w("=" * 100)
    w("")
    if metadata.api:
        w(f"  Graphics API:      {metadata.api}")
    if metadata.driver_info:
        w(f"  Driver Info:       {metadata.driver_info}")
    if metadata.machine_info:
        w(f"  Machine Info:      {metadata.machine_info}")
    if metadata.renderdoc_version:
        w(f"  RenderDoc Version: {metadata.renderdoc_version}")

    # Extract from sections
    for sec_name, strings in section_strings.items():
        if strings:
            w(f"")
            w(f"  Embedded Section '{sec_name}':")
            for s in strings[:50]:
                w(f"    {s}")
    w("")

    # ── Section 2: Frame Overview ──
    w("=" * 100)
    w("SECTION 2: FRAME OVERVIEW & STATISTICS")
    w("=" * 100)
    w("")
    w(f"  Total API Calls:              {stats.total_api_calls:>10,}")
    w(f"  Draw Calls:                   {stats.total_draw_calls:>10,}")
    w(f"  Compute Dispatches:           {stats.total_dispatches:>10,}")
    w(f"  Pipeline Barriers:            {stats.total_barriers:>10,}")
    w(f"  Render Passes:                {stats.total_render_passes:>10,}")
    w(f"  Resources Created:            {stats.total_resources_created:>10,}")
    w(f"  Descriptor Set Updates:       {stats.total_descriptor_updates:>10,}")
    w(f"  Push Constant Calls:          {stats.total_push_constant_calls:>10,}")
    w(f"  Copy/Transfer Operations:     {stats.total_copy_calls:>10,}")
    w(f"  Clear/Fill Operations:        {stats.total_clear_calls:>10,}")
    w(f"  Query Operations:             {stats.total_query_calls:>10,}")
    w(f"  Queue Submit/Present:         {stats.total_submit_calls:>10,}")
    w("")

    # API call frequency table
    if api_calls:
        call_freq = defaultdict(int)
        for call in api_calls:
            call_freq[call.name] += 1

        w("  API Call Frequency Table:")
        w(f"  {'Function':<55s} {'Count':>8s} {'Category':>15s}")
        w(f"  {'-'*55} {'-'*8} {'-'*15}")
        for func, count in sorted(call_freq.items(), key=lambda x: -x[1]):
            category = 'other'
            for cat_name, pattern in VK_CALL_CATEGORIES.items():
                if pattern.search(func):
                    category = cat_name
                    break
            w(f"  {func:<55s} {count:>8d} {category:>15s}")
        w("")

    # ── Section 3: Render Pass Structure ──
    w("=" * 100)
    w("SECTION 3: RENDER PASS STRUCTURE")
    w("=" * 100)
    w("")

    # Reconstruct render pass hierarchy from API calls
    render_passes = []
    current_rp = None
    rp_draw_count = 0

    for call in api_calls:
        if 'BeginRender' in call.name or 'BeginRenderPass' in call.name:
            if current_rp:
                current_rp['draw_count'] = rp_draw_count
                render_passes.append(current_rp)
            current_rp = {
                'begin_event': call.event_id,
                'name': call.name,
                'params': call.parameters,
                'draw_count': 0,
                'dispatches': 0,
                'barriers': 0,
            }
            rp_draw_count = 0
        elif 'EndRender' in call.name or 'EndRenderPass' in call.name:
            if current_rp:
                current_rp['draw_count'] = rp_draw_count
                current_rp['end_event'] = call.event_id
                render_passes.append(current_rp)
                current_rp = None
                rp_draw_count = 0
        elif current_rp:
            if call.category == 'draw':
                rp_draw_count += 1
            elif call.category == 'dispatch':
                current_rp['dispatches'] = current_rp.get('dispatches', 0) + 1
            elif call.category == 'sync':
                current_rp['barriers'] = current_rp.get('barriers', 0) + 1

    if render_passes:
        w(f"  Total Render Passes: {len(render_passes)}")
        w("")
        for i, rp in enumerate(render_passes):
            w(f"  Render Pass #{i+1}:")
            w(f"    Begin Event:     {rp['begin_event']}")
            end_event = rp.get('end_event', 'N/A')
            w(f"    End Event:       {end_event}")
            w(f"    Draw Calls:      {rp['draw_count']}")
            w(f"    Dispatches:      {rp.get('dispatches', 0)}")
            w(f"    Barriers:        {rp.get('barriers', 0)}")
            if rp['params'] and verbosity in ('normal', 'verbose'):
                w(f"    Parameters:")
                for k, v in rp['params'].items():
                    w(f"      {k}: {v}")
            w("")
    else:
        w("  No render pass begin/end pairs detected in API stream.")
        w("  (The capture may use dynamic rendering or the XML format")
        w("   may encode render passes differently.)")
    w("")

    # ── Section 4: Queue Submissions ──
    w("=" * 100)
    w("SECTION 4: QUEUE SUBMISSIONS & PRESENTATION")
    w("=" * 100)
    w("")

    submits = [c for c in api_calls if c.category == 'submit']
    if submits:
        w(f"  Total Queue Operations: {len(submits)}")
        w("")
        for i, sub in enumerate(submits):
            w(f"  [{i+1}] {sub.name} (Event #{sub.event_id})")
            if sub.parameters and verbosity in ('normal', 'verbose'):
                for k, v in sub.parameters.items():
                    w(f"       {k}: {v}")
        w("")
    else:
        w("  No queue submit/present calls detected.")
    w("")

    # ── Section 5: Draw Call Details ──
    w("=" * 100)
    w("SECTION 5: DRAW CALL DETAILS")
    w("=" * 100)
    w("")

    draws = [c for c in api_calls if c.category == 'draw']
    if draws:
        # Group by draw call type
        draw_types = defaultdict(list)
        for d in draws:
            draw_types[d.name].append(d)

        w(f"  Total Draw Calls: {len(draws)}")
        w("")
        w(f"  Draw Call Types:")
        for dtype, dcalls in sorted(draw_types.items(), key=lambda x: -len(x[1])):
            w(f"    {dtype:<50s} x{len(dcalls)}")
        w("")

        if verbosity in ('normal', 'verbose'):
            limit = 100 if verbosity == 'normal' else 1000
            w(f"  Draw Call Parameters (first {min(limit, len(draws))}):")
            w("")
            for i, d in enumerate(draws[:limit]):
                w(f"  [{d.event_id:6d}] {d.name}")
                if d.parameters:
                    for k, v in d.parameters.items():
                        w(f"           {k}: {v}")
            if len(draws) > limit:
                w(f"  ... and {len(draws) - limit} more draw calls")
            w("")
    else:
        w("  No draw calls detected.")
    w("")

    # ── Section 6: Compute Dispatches ──
    w("=" * 100)
    w("SECTION 6: COMPUTE DISPATCHES")
    w("=" * 100)
    w("")

    dispatches = [c for c in api_calls if c.category == 'dispatch']
    if dispatches:
        dispatch_types = defaultdict(list)
        for d in dispatches:
            dispatch_types[d.name].append(d)

        w(f"  Total Compute Dispatches: {len(dispatches)}")
        w("")
        w(f"  Dispatch Types:")
        for dtype, dcalls in sorted(dispatch_types.items(), key=lambda x: -len(x[1])):
            w(f"    {dtype:<50s} x{len(dcalls)}")
        w("")

        if verbosity in ('normal', 'verbose'):
            limit = 50 if verbosity == 'normal' else 500
            w(f"  Dispatch Parameters (first {min(limit, len(dispatches))}):")
            for d in dispatches[:limit]:
                w(f"  [{d.event_id:6d}] {d.name}")
                if d.parameters:
                    for k, v in d.parameters.items():
                        w(f"           {k}: {v}")
            w("")
    else:
        w("  No compute dispatches detected.")
    w("")

    # ── Section 7: Pipeline Barriers & Synchronization ──
    w("=" * 100)
    w("SECTION 7: PIPELINE BARRIERS & SYNCHRONIZATION")
    w("=" * 100)
    w("")

    barriers = [c for c in api_calls if c.category == 'sync']
    if barriers:
        barrier_types = defaultdict(int)
        for b in barriers:
            barrier_types[b.name] += 1

        w(f"  Total Barrier/Sync Operations: {len(barriers)}")
        w("")
        w(f"  Barrier Types:")
        for btype, count in sorted(barrier_types.items(), key=lambda x: -x[1]):
            w(f"    {btype:<50s} x{count}")
        w("")

        if verbosity == 'verbose':
            w(f"  Barrier Details (first 200):")
            for b in barriers[:200]:
                w(f"  [{b.event_id:6d}] {b.name}")
                if b.parameters:
                    for k, v in b.parameters.items():
                        w(f"           {k}: {v}")
            w("")
    else:
        w("  No barrier/sync operations detected.")
    w("")

    # ── Section 8: Descriptor Sets & Bindings ──
    w("=" * 100)
    w("SECTION 8: DESCRIPTOR SETS & BINDINGS")
    w("=" * 100)
    w("")

    desc_calls = [c for c in api_calls if c.category == 'descriptor']
    binding_calls = [c for c in api_calls if c.category == 'binding']

    if desc_calls:
        desc_types = defaultdict(int)
        for d in desc_calls:
            desc_types[d.name] += 1

        w(f"  Total Descriptor Operations: {len(desc_calls)}")
        for dtype, count in sorted(desc_types.items(), key=lambda x: -x[1]):
            w(f"    {dtype:<50s} x{count}")
        w("")

    if binding_calls:
        bind_types = defaultdict(int)
        for b in binding_calls:
            bind_types[b.name] += 1

        w(f"  Total Binding Operations: {len(binding_calls)}")
        for btype, count in sorted(bind_types.items(), key=lambda x: -x[1]):
            w(f"    {btype:<50s} x{count}")
        w("")

    if not desc_calls and not binding_calls:
        w("  No descriptor/binding operations detected.")
    w("")

    # ── Section 9: Resource Creation ──
    w("=" * 100)
    w("SECTION 9: RESOURCE CREATION")
    w("=" * 100)
    w("")

    creation_calls = [c for c in api_calls if c.category == 'creation']
    if creation_calls:
        create_types = defaultdict(int)
        for c in creation_calls:
            create_types[c.name] += 1

        w(f"  Total Resource Creation Calls: {len(creation_calls)}")
        w("")
        w(f"  Creation Types:")
        for ctype, count in sorted(create_types.items(), key=lambda x: -x[1]):
            w(f"    {ctype:<50s} x{count}")
        w("")

        if verbosity in ('normal', 'verbose'):
            limit = 200 if verbosity == 'normal' else 2000
            w(f"  Resource Creation Details (first {min(limit, len(creation_calls))}):")
            for c in creation_calls[:limit]:
                w(f"  [{c.event_id:6d}] {c.name}")
                if c.parameters:
                    for k, v in c.parameters.items():
                        w(f"           {k}: {v}")
            w("")
    else:
        w("  No resource creation calls detected.")

    if resources:
        w("")
        w(f"  Tracked Resources ({len(resources)}):")
        res_by_type = defaultdict(list)
        for r in resources:
            res_by_type[r.resource_type].append(r)

        for rtype, rlist in sorted(res_by_type.items()):
            w(f"    {rtype}: {len(rlist)} resources")
            if verbosity in ('normal', 'verbose'):
                for r in rlist[:20]:
                    detail_str = ", ".join(f"{k}={v}" for k, v in r.details.items() if v) if r.details else ""
                    w(f"      [{r.handle or 'N/A'}] {r.name or '(unnamed)'} {detail_str}")
    w("")

    # ── Section 10: Copy & Transfer Operations ──
    w("=" * 100)
    w("SECTION 10: COPY & TRANSFER OPERATIONS")
    w("=" * 100)
    w("")

    copies = [c for c in api_calls if c.category in ('copy', 'transfer')]
    if copies:
        copy_types = defaultdict(int)
        for c in copies:
            copy_types[c.name] += 1

        w(f"  Total Copy/Transfer Operations: {len(copies)}")
        for ctype, count in sorted(copy_types.items(), key=lambda x: -x[1]):
            w(f"    {ctype:<50s} x{count}")
        w("")
    else:
        w("  No copy/transfer operations detected.")
    w("")

    # ── Section 11: Dynamic State ──
    w("=" * 100)
    w("SECTION 11: DYNAMIC STATE CHANGES")
    w("=" * 100)
    w("")

    state_calls = [c for c in api_calls if c.category == 'state']
    if state_calls:
        state_types = defaultdict(int)
        for s in state_calls:
            state_types[s.name] += 1

        w(f"  Total Dynamic State Changes: {len(state_calls)}")
        for stype, count in sorted(state_types.items(), key=lambda x: -x[1]):
            w(f"    {stype:<50s} x{count}")
    else:
        w("  No dynamic state changes detected.")
    w("")

    # ── Section 12: Chrome JSON Event Data ──
    if chrome_data and chrome_data.get('events'):
        w("=" * 100)
        w("SECTION 12: EVENT TIMELINE (from Chrome JSON export)")
        w("=" * 100)
        w("")

        w(f"  Total Timeline Events: {len(chrome_data['events']):,}")
        if chrome_data['total_duration_us'] > 0:
            w(f"  Total Duration:        {chrome_data['total_duration_us'] / 1000:.2f} ms")
        w("")

        if chrome_data['categories']:
            w("  Event Categories:")
            for cat, count in sorted(chrome_data['categories'].items(), key=lambda x: -x[1]):
                w(f"    {cat or '(uncategorized)':<40s} {count:>8d}")
            w("")

        if verbosity in ('normal', 'verbose'):
            limit = 200 if verbosity == 'normal' else 2000
            w(f"  Event Details (first {min(limit, len(chrome_data['events']))}):")
            for ev in chrome_data['events'][:limit]:
                dur_str = f" ({ev['duration_us']/1000:.3f}ms)" if ev['duration_us'] else ""
                w(f"    [{ev['timestamp_us']:>12d}us] {ev['name']}{dur_str}")
                if verbosity == 'verbose' and ev.get('args'):
                    for k, v in ev['args'].items():
                        w(f"                     {k}: {v}")
            w("")

    # ── Section 13: Complete API Call Stream ──
    if verbosity == 'verbose' and api_calls:
        w("=" * 100)
        w("SECTION 13: COMPLETE API CALL STREAM (chronological)")
        w("=" * 100)
        w("")
        w(f"  Total calls: {len(api_calls):,}")
        w("")

        for call in api_calls:
            param_str = ""
            if call.parameters:
                params = [f"{k}={v}" for k, v in list(call.parameters.items())[:10]]
                param_str = f" ({', '.join(params)})"
            w(f"  [{call.event_id:8d}] [{call.category:>15s}] {call.name}{param_str}")
        w("")

    # ── Summary ──
    w("=" * 100)
    w("EXTRACTION SUMMARY")
    w("=" * 100)
    w("")
    w(f"  File:                    {os.path.basename(rdc_path)}")
    w(f"  File Size:               {os.path.getsize(rdc_path):,} bytes")
    w(f"  API Calls Extracted:     {len(api_calls):,}")
    w(f"  Resources Tracked:       {len(resources):,}")
    w(f"  Render Passes:           {len(render_passes)}")
    w(f"  Sections Extracted:      {len(section_strings)}")
    w("")

    w("  IMPORTANT NOTES FOR AI ANALYSIS:")
    w("  ─────────────────────────────────")
    w("  1. RenderDoc is a DEBUGGER, not a profiler. This capture contains the")
    w("     complete API call stream and GPU state, but NOT performance timing data.")
    w("     For GPU timing, use NVIDIA Nsight Graphics GPU Trace instead.")
    w("  2. Resource contents (buffer data, texture pixels) are stored in the binary")
    w("     RDC format and require the RenderDoc Replay API for extraction.")
    w("  3. Shader source/SPIR-V bytecode is embedded but requires the RenderDoc")
    w("     Python API (renderdoc.pyd) to extract. This script extracts metadata only.")
    w("  4. For pipeline state inspection at specific draw calls, use the RenderDoc GUI")
    w("     or write a custom script using the RenderDoc Python Replay API.")
    w("")

    w("=" * 100)
    w("END OF EXTRACTION")
    w("=" * 100)

    return "\n".join(lines)


# ─── SPIR-V Shader Parsing ───────────────────────────────────────────────────

def parse_spirv_shaders(temp_dir: str) -> list:
    """Scan temp directory or extracted capture for SPIR-V shader binaries."""
    shaders = []
    if not os.path.isdir(temp_dir):
        return shaders

    STAGE_NAMES = {
        0: 'Vertex', 1: 'TessControl', 2: 'TessEval', 3: 'Geometry',
        4: 'Fragment', 5: 'GLCompute', 5267: 'TaskNV', 5268: 'MeshNV',
        5364: 'TaskEXT', 5365: 'MeshEXT'
    }

    for root, _, files in os.walk(temp_dir):
        for fname in files:
            fpath = os.path.join(root, fname)
            try:
                if os.path.getsize(fpath) >= 20:
                    with open(fpath, 'rb') as f:
                        data = f.read()
                    if len(data) >= 20 and data[:4] == b'\x03\x02\x23\x07':
                        ver = struct.unpack('<I', data[4:8])[0]
                        maj = (ver >> 16) & 0xff
                        min_v = (ver >> 8) & 0xff
                        bound = struct.unpack('<I', data[12:16])[0]
                        entry_points = []
                        idx = 20
                        while idx + 4 <= len(data):
                            w = struct.unpack('<I', data[idx:idx+4])[0]
                            word_count = w >> 16
                            opcode = w & 0xffff
                            if word_count == 0 or idx + word_count * 4 > len(data):
                                break
                            if opcode == 15 and word_count >= 4:
                                exec_model = struct.unpack('<I', data[idx+4:idx+8])[0]
                                stage = STAGE_NAMES.get(exec_model, f"Stage({exec_model})")
                                str_bytes = data[idx+12:idx+word_count*4]
                                null_pos = str_bytes.find(b'\x00')
                                name = str_bytes[:null_pos].decode('utf-8', errors='replace') if null_pos != -1 else "main"
                                entry_points.append({'stage': stage, 'name': name})
                            idx += word_count * 4

                        shaders.append({
                            'file': fname,
                            'version': f"{maj}.{min_v}",
                            'size': len(data),
                            'entry_points': entry_points,
                        })
            except Exception:
                pass

    return shaders


# ─── Diff Comparison ───────────────────────────────────────────────────────────

def generate_diff_output(file1: str, file2: str) -> str:
    """Compare two capture analysis files side-by-side."""
    lines = []
    w = lines.append
    w("=" * 100)
    w("CAPTURE DIFF COMPARISON SUMMARY")
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
                if 'API Call Frequency Table:' in line or 'API Call Summary:' in line:
                    in_table = True
                    continue
                if in_table and '=====' in line:
                    break
                if in_table and line.strip() and not line.startswith('Function'):
                    parts = line.split()
                    if len(parts) >= 2 and parts[-2].isdigit():
                        func = parts[0]
                        counts[func] = int(parts[-2])
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
        description="Convert RenderDoc Capture (.rdc) to AI-readable text.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python renderdoc_rdc_to_text.py capture.rdc
  python renderdoc_rdc_to_text.py capture.rdc --output analysis.txt
  python renderdoc_rdc_to_text.py capture.rdc --diff other_analysis.txt
  python renderdoc_rdc_to_text.py capture.rdc --renderdoccmd "C:\\Path\\To\\renderdoccmd.exe"
        """
    )
    parser.add_argument("input", help="Path to .rdc file or previous analysis text file")
    parser.add_argument("--output", "-o", help="Output text file path (default: <input>_analysis.txt)")
    parser.add_argument("--verbosity", "-v", choices=["summary", "normal", "verbose"],
                        default="normal", help="Output verbosity level (default: normal)")
    parser.add_argument("--renderdoccmd", help="Path to renderdoccmd.exe")
    parser.add_argument("--keep-temp", action="store_true",
                        help="Keep temporary XML/JSON files (for debugging)")
    parser.add_argument("--xml-only", help="Use pre-existing XML file instead of converting from RDC")
    parser.add_argument("--diff", help="Path to second capture analysis file to diff against input")

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

    renderdoccmd = find_renderdoccmd(args.renderdoccmd)
    print(f"Using renderdoccmd: {renderdoccmd}")
    print(f"Input: {input_path} ({input_path.stat().st_size / 1073741824:.2f} GB)")
    print(f"Output: {output_path}")
    print(f"Verbosity: {args.verbosity}")
    print()

    # Create temp directory for intermediate files
    temp_dir = str(input_path.parent / f".{input_path.stem}_rdc_temp")
    os.makedirs(temp_dir, exist_ok=True)

    try:
        # Step 1: Convert RDC to XML
        if args.xml_only:
            xml_path = args.xml_only
            print(f"Using pre-existing XML: {xml_path}")
        else:
            print("Step 1/5: Converting RDC to XML...")
            xml_path = convert_rdc_to_xml(str(input_path), renderdoccmd, temp_dir)

        # Step 2: Also export Chrome JSON
        print("Step 2/5: Exporting Chrome JSON for event timeline...")
        chrome_json_path = convert_rdc_to_chrome_json(str(input_path), renderdoccmd, temp_dir)

        # Step 3: Extract embedded sections
        print("Step 3/5: Extracting embedded sections...")
        sections = extract_sections(str(input_path), renderdoccmd, temp_dir)
        section_strings = parse_section_strings(sections)

        # Step 4: Extract thumbnail & SPIR-V shaders
        print("Step 4/5: Extracting thumbnail and scanning SPIR-V shaders...")
        thumbnail_path = extract_thumbnail(str(input_path), renderdoccmd, temp_dir)
        spirv_shaders = parse_spirv_shaders(temp_dir)
        if spirv_shaders:
            print(f"  Extracted {len(spirv_shaders)} SPIR-V shader modules")

        # Step 5: Parse XML
        print("Step 5/5: Parsing extracted data...")
        metadata = RDCMetadata(
            file_path=str(input_path),
            file_size=input_path.stat().st_size,
        )

        api_calls = []
        resources = []
        stats = FrameStats()

        if xml_path and os.path.exists(xml_path):
            api_calls, resources, stats, xml_metadata = parse_xml_streaming(xml_path, args.verbosity)
            if xml_metadata.api:
                metadata.api = xml_metadata.api
            if xml_metadata.driver_info:
                metadata.driver_info = xml_metadata.driver_info

        chrome_data = parse_chrome_json(chrome_json_path)

        # Generate output
        print("Generating output...")
        output_text = generate_output(
            str(input_path), metadata, stats, api_calls, resources,
            chrome_data, section_strings, thumbnail_path, args.verbosity,
        )

        # Append SPIR-V shaders summary if found
        if spirv_shaders:
            sh_lines = ["\n" + "="*100, "SECTION 14: DISCOVERED SPIR-V SHADER MODULES", "="*100, ""]
            for sh in spirv_shaders:
                eps = ", ".join(f"{ep['stage']}:{ep['name']}" for ep in sh['entry_points']) or "No entry point"
                sh_lines.append(f"  [{sh['file']}] SPIR-V {sh['version']} ({sh['size']:,} bytes) — Entry Points: {eps}")
            sh_lines.append("")
            output_text += "\n" + "\n".join(sh_lines)

        # Write output
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(output_text)

        line_count = output_text.count('\n')
        print(f"\nDone! Output: {output_path}")
        print(f"  Size: {os.path.getsize(output_path):,} bytes")
        print(f"  Lines: {line_count:,}")
        print(f"  API Calls: {len(api_calls):,}")
        print(f"  Resources: {len(resources):,}")

        # Copy thumbnail next to output if it exists
        if thumbnail_path:
            thumb_dest = output_path.with_suffix('.thumb.jpg')
            shutil.copy2(thumbnail_path, thumb_dest)
            print(f"  Thumbnail: {thumb_dest}")

    finally:
        # Cleanup temp files unless --keep-temp
        if not args.keep_temp and os.path.isdir(temp_dir):
            try:
                shutil.rmtree(temp_dir)
                print(f"  Cleaned up temp directory: {temp_dir}")
            except OSError:
                pass


if __name__ == "__main__":
    main()

