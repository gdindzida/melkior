# melkior
vulkan based rpi 5 videocore vii compute library

## build & run

```
mkdir build
cd build
cmake ..
make -j
./vulkan_compute
```

## progress

### Level -1

Key concepts you should learn (compute-first)
1) Instance → Physical device → Logical device

What it is: selecting a GPU, enabling features, and creating a device handle.

Instance = connection to Vulkan runtime

PhysicalDevice = a real GPU (on Pi 5: likely Mesa/V3D driver)

Device = what you actually use to create buffers, pipelines, etc.

Practice: Write a “hello device” app that prints:

device name, vendor, limits (max workgroup size, shared memory, etc.)

supported extensions (important on embedded)

2) Queues and command buffers

What it is: you don’t “call a kernel.” You record commands into a command buffer and submit to a queue.
For compute you’ll likely use a compute queue (sometimes it’s the same family as graphics).

Practice: Record a command buffer that:

fills a buffer with a value (vkCmdFillBuffer)

copies buffer A→B (vkCmdCopyBuffer)

No shaders yet—just get comfortable with “record then submit”.

3) Buffers vs images (for GEMM/FFT you mostly want buffers)

What it is: GPU memory resources.

GEMM/FFT: usually VkBuffer with storage buffer access

Images are more for textures / some specialized FFT approaches, but start with buffers.

Important subtopic: memory allocation

Vulkan exposes memory types; you choose and allocate.

On Linux you’ll almost certainly want a helper allocator (see VMA below).

Practice: Allocate 3 buffers (A, B, C), map and write input data, read output.

4) Descriptor sets (how shaders “see” your buffers)

What it is: the binding model.

You define a descriptor set layout (e.g., binding 0 = A, binding 1 = B, binding 2 = C)

Allocate a descriptor set and update it with your VkBuffer infos.

Practice: Make a trivial compute shader that does C[i] = A[i] + B[i].
Hook it up with descriptor sets.

5) Compute pipelines and SPIR-V

What it is:

Vulkan runs shaders compiled to SPIR-V

A compute pipeline packages the compute shader + layout (descriptor/push constants)

Practice: One pipeline, one dispatch:

compile GLSL compute shader → SPIR-V

create VkShaderModule, VkPipelineLayout, VkPipeline

dispatch vkCmdDispatch

6) Workgroups, local size, occupancy

What it is: compute shaders run in a 3D grid.

local_size_x/y/z = threads per workgroup (shared memory is per-workgroup)

vkCmdDispatch(x,y,z) = number of workgroups

For GEMM/FFT performance this is huge:

tiling

shared memory use

vectorized loads

avoiding bank conflicts / divergence

Practice: Benchmark different local_size_x values (like 64/128/256) for a simple kernel and graph it.

7) Synchronization: barriers and hazards

What it is: Vulkan won’t protect you from reading stale data.
For compute you’ll care about:

pipeline barriers (VkBufferMemoryBarrier2 / vkCmdPipelineBarrier2)

queue submission fences (CPU waits for GPU)

semaphores (GPU-GPU ordering across queues; less needed if you keep it simple)

Rule of thumb: anytime you write a buffer then read it in another dispatch/copy, you need the right barrier/order.

Practice: Chain two kernels: kernel1 writes buffer, kernel2 reads it. Add a barrier between them. Then remove it and observe corruption (often you’ll see it).

8) Specialization constants and push constants

What it is:

push constants: tiny fast parameters (like matrix sizes, strides)

specialization constants: compile-time-ish constants without recompiling SPIR-V

For GEMM/FFT you’ll use push constants for sizes and offsets.

Practice: Implement GEMM with runtime M/N/K passed in push constants, verify correctness.

9) Limits and portability constraints (important on Pi 5)

You’ll need to design around:

max workgroup invocations

shared memory size (maxComputeSharedMemorySize)

storage buffer alignment rules

subgroup support (wave/warp operations) varies

Practice: Print VkPhysicalDeviceLimits and adapt your kernel tile sizes accordingly.


### Level 0 — Sanity & Infrastructure

- (“GPU is alive” kernels)
- Buffer clear (out[i] = c)
- Buffer copy (out[i] = in[i])
- Strided copy (out[i] = in[i * stride])
- Indexed gather (out[i] = in[idx[i]])

#### Core concepts

- Dispatch geometry
- Bounds checks
- Memory correctness
- Baseline bandwidth

### Level 1 — Elementwise (Embarrassingly Parallel) (Chapter 2)

- Vector add (C = A + B)
- SAXPY / AXPY
- Elementwise nonlinear ops (ReLU, tanh, sigmoid)
- Masked elementwise ops (branch divergence toy)
- Elementwise clamp / normalization

#### Core concepts

- Thread ↔ data mapping
- Divergence
- Instruction throughput
- Memory-bound vs compute-bound

### Level 2 — Multidimensional Mapping (Chapter 3)

- 2D buffer copy (images with stride/pitch)
- 2D transpose (naive)
- 2D transpose (tiled, shared memory)
- Naive matrix multiplication
- Tiled matrix multiplication

#### Core concepts

- 2D/3D indexing
- Memory coalescing
- Shared memory tiling
- Arithmetic intensity

### Level 3 — Memory & Architecture Experiments (Chapters 4–6)

- Coalesced vs strided vs random access kernels
- Occupancy experiments (register/shared pressure)
- Thread coarsening (multiple elements per thread)
- Kernel fusion (elementwise + reduction)
- Latency hiding experiments

#### Core concepts

- Occupancy
- Scheduling
- Register pressure
- Memory latency hiding

### Level 4 — Convolution (Chapter 7)

- 1D convolution (naive)
- 1D convolution (constant/UBO weights)
- 2D convolution (naive)
- 2D convolution with shared-memory tiling
- 2D convolution with halo cells

#### Core concepts

- Data reuse
- Halo regions
- Constant vs global memory behavior
- Cache effectiveness

### Level 5 — Stencil Computations (Chapter 8)

- 2D 5-point stencil (Jacobi iteration)
- 2D stencil with shared-memory tiling
- Thread-coarsened stencil sweep
- Register-tiled stencil
- 3D 7-point stencil

#### Core concepts

- Iterative kernels
- Synchronization
- Strongly memory-bound workloads
- Spatial locality

### Level 6 — Atomics & Irregular Updates (Chapter 9)

- Global atomic counter
- Global histogram
- Privatized histogram (shared memory)
- Aggregated histogram (reduced contention)
- Scatter with atomics

#### Core concepts

- Atomic contention
- Serialization
- Privatization
- Throughput vs latency

### Level 7 — Reductions (Chapter 10)

- Sum reduction (naive)
- Tree-based reduction
- Divergence-minimized reduction
- Coarsened reduction
- Hierarchical (multi-dispatch) reduction
- Min/max + argmin
- Dot product
 
#### Core concepts

- Parallel trees
- Synchronization
- Memory traffic minimization
- Multi-stage algorithms

### Level 8 — Prefix Sum (Scan) (Chapter 11)

- Inclusive scan (Kogge–Stone)
- Exclusive scan
- Work-efficient scan (Brent–Kung)
- Block-### level scan
- Full-array hierarchical scan
- Segmented scan

#### Core concepts

- Work efficiency
- Algorithmic optimality
- Synchronization patterns
- Foundation for many higher algorithms

### Level 9 — Stream Compaction & Filtering

- Flag generation
- Prefix sum on flags
- Scatter into compacted output
- Stable compaction
- Partitioning (true/false split)

#### Core concepts

- Composition of primitives
- Dynamic output size
- Memory movement costs

### Level 10 — Merge & Sorting Primitives (Chapter 12)

- Merge two sorted arrays
- Parallel merge (merge-path / co-rank)
- Tiled merge
- Bottom-up merge sort
- Bitonic sort (local)
- Hybrid sort (bitonic + merge)

#### Core concepts

- Load balancing
- Irregular access
- Comparison networks
- Multi-pass pipelines

### Level 11 — Histogram + Scan Compositions

- Radix sort (LSD)
- Multi-pass radix sort (4-bit → 8-bit digits)
- Sparse matrix assembly (COO → CSR)
- Binning / spatial hashing
- Particle binning

#### Core concepts

- Algorithm composition
- Pipeline orchestration
- Memory ping-pong
- Global synchronization via dispatches

### Level 12 — Graph & Irregular Algorithms

- BFS (frontier-based)
- Graph coloring
- Connected components (label propagation)
- SpMV (CSR format)
- PageRank iteration

#### Core concepts

- Irregular parallelism
- Atomic-heavy workloads
- Dynamic frontiers
- Convergence loops

### Level 13 — FFT (Capstone Level)

- 1D FFT (radix-2, small size, single workgroup)
- Batched FFTs (many small signals)
- FFT with shared-memory staging
- In-place FFT
- Stockham autosort FFT
- Multi-dispatch FFT (large sizes)

#### Core concepts
- Complex arithmetic
- Butterfly patterns
- Strided + permuted access
- Synchronization-heavy computation
- Compute-bound kernels