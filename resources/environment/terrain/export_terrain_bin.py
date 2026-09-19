# Copyright © 2026 spacegirl65. All Rights Reserved.
"""
Exports merged 15km x 15km LiDAR DTM tiles into a native binary mesh format
(.mesh) consumable directly by Sandbox3D's Renderer::Mesh and TerrainMesh loader.
"""

import os
import struct
import numpy as np

def export_dtm_to_mesh(tiles_dir=".", output_path="terrain_15km.mesh", step=15):
    try:
        import tifffile
    except ImportError:
        print("tifffile is required to read GeoTIFF files: pip install tifffile imagecodecs")
        return

    tiles = {}
    for fn in os.listdir(tiles_dir):
        if not fn.startswith('lidar_composite_dtm-'):
            continue
        osgrid = fn[-6:]
        tif_path = os.path.join(tiles_dir, fn, f"{osgrid}_DTM_1m.tif")
        tfw_path = os.path.join(tiles_dir, fn, f"{osgrid}_DTM_1m.tfw")
        if not os.path.exists(tif_path) or not os.path.exists(tfw_path):
            continue

        with open(tfw_path) as f:
            lines = [float(line.strip()) for line in f if line.strip()]
        pix_x, _, _, pix_y, x_ul, y_ul = lines

        data = tifffile.imread(tif_path)
        h, w = data.shape

        left = x_ul - 0.5 * pix_x
        top = y_ul - 0.5 * pix_y
        right = left + w * pix_x
        bottom = top + h * pix_y

        tiles[osgrid] = {
            'data': data,
            'left': left,
            'right': right,
            'bottom': bottom,
            'top': top
        }
        print(f"Loaded {fn} with shape {data.shape}, bounds: [{left:.0f}, {bottom:.0f}] to [{right:.0f}, {top:.0f}]")

    if not tiles:
        print("No LiDAR tiles found in specified directory.")
        return

    min_left = min(tile['left'] for tile in tiles.values())
    min_bottom = min(tile['bottom'] for tile in tiles.values())
    max_right = max(tile['right'] for tile in tiles.values())
    max_top = max(tile['top'] for tile in tiles.values())

    width_m = max_right - min_left
    depth_m = max_top - min_bottom
    print(f"Total bounds: [{min_left:.0f}, {min_bottom:.0f}] to [{max_right:.0f}, {max_top:.0f}] ({width_m:.0f}m x {depth_m:.0f}m)")

    full_w = int(width_m)
    full_h = int(depth_m)
    array = np.empty((full_h, full_w), dtype=np.float32)

    for osgrid, tile in tiles.items():
        left = int(tile['left'] - min_left)
        bottom = int(tile['bottom'] - min_bottom)
        w = tile['data'].shape[1]
        h = tile['data'].shape[0]
        array[bottom:bottom + h, left:left + w] = tile['data'][::-1]

    # Downsample by step (e.g. 15m step -> 1000 x 1000 vertices)
    elev = array[::step, ::step].astype(np.float32)
    res_z, res_x = elev.shape
    valid_mask = elev > -999.0
    min_elev = float(np.nanmin(elev[valid_mask])) if np.any(valid_mask) else 0.0
    max_elev = float(np.nanmax(elev[valid_mask])) if np.any(valid_mask) else 1000.0
    print(f"Downsampled grid: {res_x} x {res_z}, elevation range: {min_elev:.1f}m - {max_elev:.1f}m")

    # Replace invalid / nodata values with minimum elevation
    elev[~valid_mask] = min_elev

    # Generate vertex buffer data
    half_w = width_m * 0.5
    half_d = depth_m * 0.5
    step_x = width_m / (res_x - 1)
    step_z = depth_m / (res_z - 1)

    # Compute central-difference surface normals
    dz, dx = np.gradient(elev, step_z, step_x)
    norm_x = -dx
    norm_y = np.ones_like(elev)
    norm_z = -dz
    norm_len = np.sqrt(norm_x * norm_x + norm_y * norm_y + norm_z * norm_z)
    norm_x /= norm_len
    norm_y /= norm_len
    norm_z /= norm_len

    # British landscape palette tinting
    slope_factor = np.clip(1.0 - norm_y, 0.0, 1.0)
    height_ratio = np.clip((elev - min_elev) / max(max_elev - min_elev, 1.0), 0.0, 1.0)

    # Palette colours: valley pasture (0.28, 0.42, 0.22), high moorland (0.38, 0.35, 0.26), steep scar (0.48, 0.46, 0.44)
    color_r = np.where(slope_factor > 0.45, 0.48, np.where(height_ratio > 0.65, 0.38, 0.28)).astype(np.float32)
    color_g = np.where(slope_factor > 0.45, 0.46, np.where(height_ratio > 0.65, 0.35, 0.42)).astype(np.float32)
    color_b = np.where(slope_factor > 0.45, 0.44, np.where(height_ratio > 0.65, 0.26, 0.22)).astype(np.float32)
    color_a = np.ones_like(color_r, dtype=np.float32)

    # Local vertex positions centred at (0, 0, 0)
    x_coords = np.linspace(-half_w, half_w, res_x, dtype=np.float32)
    z_coords = np.linspace(-half_d, half_d, res_z, dtype=np.float32)
    pos_x, pos_z = np.meshgrid(x_coords, z_coords)
    pos_y = elev.astype(np.float32)

    vertex_count = res_x * res_z

    # Flatten vertex attributes
    # Vertex struct: float pos[3], float norm[3], float col[4] -> 10 floats = 40 bytes
    vertices = np.zeros((vertex_count, 10), dtype=np.float32)
    vertices[:, 0] = pos_x.ravel()
    vertices[:, 1] = pos_y.ravel()
    vertices[:, 2] = pos_z.ravel()
    vertices[:, 3] = norm_x.ravel()
    vertices[:, 4] = norm_y.ravel()
    vertices[:, 5] = norm_z.ravel()
    vertices[:, 6] = color_r.ravel()
    vertices[:, 7] = color_g.ravel()
    vertices[:, 8] = color_b.ravel()
    vertices[:, 9] = color_a.ravel()

    # Generate 32-bit triangle indices (clockwise in left-handed coordinates)
    # Triangle 1: i0 -> i2 -> i3; Triangle 2: i0 -> i3 -> i1
    quad_count_x = res_x - 1
    quad_count_z = res_z - 1
    triangle_count = quad_count_x * quad_count_z * 2
    index_count = triangle_count * 3

    indices = np.empty((quad_count_z, quad_count_x, 6), dtype=np.uint32)
    iz = np.arange(quad_count_z, dtype=np.uint32)[:, None]
    ix = np.arange(quad_count_x, dtype=np.uint32)[None, :]
    i0 = iz * res_x + ix
    i1 = i0 + 1
    i2 = (iz + 1) * res_x + ix
    i3 = i2 + 1

    indices[:, :, 0] = i0
    indices[:, :, 1] = i2
    indices[:, :, 2] = i3
    indices[:, :, 3] = i0
    indices[:, :, 4] = i3
    indices[:, :, 5] = i1
    indices_flat = indices.ravel()

    # Compute bounding box
    min_x, max_x = float(pos_x.min()), float(pos_x.max())
    min_y, max_y = float(pos_y.min()), float(pos_y.max())
    min_z, max_z = float(pos_z.min()), float(pos_z.max())

    origin_x = (min_left + max_right) * 0.5
    origin_z = (min_bottom + max_top) * 0.5

    # Binary Mesh Header format:
    # magic: 'S3DM' (4s)
    # version: uint32 (1)
    # vertexCount: uint32
    # indexCount: uint32
    # minX, minY, minZ: 3 x float
    # maxX, maxY, maxZ: 3 x float
    # originX, originZ: 2 x double
    # width, depth: 2 x double
    # minElev, maxElev: 2 x float
    header = struct.pack(
        '<4sIIIffffffddddff',
        b'S3DM',
        1,
        vertex_count,
        index_count,
        min_x, min_y, min_z,
        max_x, max_y, max_z,
        origin_x, origin_z,
        width_m, depth_m,
        min_elev, max_elev
    )

    with open(output_path, 'wb') as f:
        f.write(header)
        vertices.tofile(f)
        indices_flat.tofile(f)

    file_size_mb = os.path.getsize(output_path) / (1024 * 1024)
    print(f"Exported {output_path} successfully ({file_size_mb:.2f} MB).")
    print(f"Vertices: {vertex_count:,}, Triangles: {triangle_count:,}, Indices: {index_count:,}")
    print(f"Bounding Box: [{min_x:.1f}, {min_y:.1f}, {min_z:.1f}] to [{max_x:.1f}, {max_y:.1f}, {max_z:.1f}]")

if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    output_mesh = os.path.join(script_dir, "terrain_15km.mesh")
    export_dtm_to_mesh(tiles_dir=script_dir, output_path=output_mesh)
