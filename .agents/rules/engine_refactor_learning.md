---
trigger: always_on
description: Prioritize in-depth explanations, pedantic learning, external resources, and comprehensive documentation when working on docs/engine_refactor_design.md
globs: ["docs/engine_refactor_design.md"]
---

# In-Depth Learning & Technical Explanation Protocol for Engine Refactor Design

When the user asks about, selects, or requests work related to [docs/engine_refactor_design.md](file:///c:/Users/Jan%20Varga/CLionProjects/RenderEngine/docs/engine_refactor_design.md):

1. **Pedantic Learning Over Fast Generation**:
   - Prioritize teaching, conceptual mastery, and deep understanding over immediate code output.
   - Deconstruct the underlying computer graphics principles, Vulkan 1.3 hardware mechanics, GPU microarchitecture impact, cache hierarchies, and memory ordering considerations.

2. **Curated Technical Links & Learning Resources**:
   - Provide high-quality external resources, including:
     - Youtuber videos
     - GDC, SIGGRAPH, and Eurographics presentations/papers.
     - GPU vendor technical blogs & optimization guides (NVIDIA Developer, GPUOpen / AMD, Intel Graphics).
     - Vulkan specifications, extension proposals, and Khronos samples.
     - Curated video lectures or conference recordings explaining the core algorithms.

3. **Comprehensive In-Depth Documents**:
   - When introducing or addressing a topic, create dedicated deep-dive design documents (or comprehensive breakdowns) exploring the whole subject in full context (not just the immediate sub-problem).
   - Cover architectural motivations, alternatives considered, edge cases, memory layout diagrams, and mathematical derivations where relevant.

4. **Never Generate Code Implementations Without Explicit Request**:
   - Never write or generate production `.cpp` implementation code unless the user explicitly asks you to do so.
   - The user implements the code themselves for mastery and learning.
   - Focus exclusively on specifications, requirements, invariants, algorithms, bit layouts, and design documentation.