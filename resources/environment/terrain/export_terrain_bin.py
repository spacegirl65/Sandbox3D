# Copyright © 2026 spacegirl65. All Rights Reserved.
"""
Exports merged 15km x 15km LiDAR DTM tiles into a compact binary heightmap format
(DTM1) consumable by Sandbox3D's TerrainMesh loader.
"""

import os
import struct
import numpy as np

def export_dtm_binary(tiles_dir=".", output_path="terrain_15km.bin", step=15):
    try:
        import rasterio
    except ImportError:
        print("rasterio is required to read GeoTIFF files: pip install rasterio")
        return

    tiles = {}
    for fn in os.listdir(tiles_dir):
        if not fn.startswith('lidar_composite_dtm-'):
            continue
        osgrid = fn[-6:]
        tif_path = os.path.join(tiles_dir, fn, f"{osgrid}_DTM_1m.tif")
        if not os.path.exists(tif_path):
            continue
        with rasterio.open(tif_path) as tile:
            data = tile.read(1)
            bounds = tile.bounds
            tiles[osgrid] = {'data': data, 'bounds': bounds}
        print(f"Loaded {fn} with shape {tiles[osgrid]['data'].shape}")

    if not tiles:
        print("No LiDAR tiles found in current directory.")
        return

    min_left = min(tile['bounds'].left for tile in tiles.values())
    min_bottom = min(tile['bounds'].bottom for tile in tiles.values())
    max_right = max(tile['bounds'].right for tile in tiles.values())
    max_top = max(tile['bounds'].top for tile in tiles.values())

    width_m = max_right - min_left
    depth_m = max_top - min_bottom
    print(f"Total bounds: [{min_left:.0f}, {min_bottom:.0f}] to [{max_right:.0f}, {max_top:.0f}] ({width_m:.0f}m x {depth_m:.0f}m)")

    full_w = int(width_m)
    full_h = int(depth_m)
    array = np.empty((full_h, full_w), dtype=np.float32)

    for osgrid, tile in tiles.items():
        left = int(tile['bounds'].left - min_left)
        bottom = int(tile['bounds'].bottom - min_bottom)
        right = left + tile['data'].shape[1]
        top = bottom + tile['data'].shape[0]
        array[bottom:top, left:right] = tile['data'][::-1]

    # Downsample by step (e.g. 15m step -> 1000 x 1000 vertices)
    sub = array[::step, ::step].astype(np.float32)
    res_z, res_x = sub.shape
    valid_mask = sub > -999.0
    min_elev = float(np.nanmin(sub[valid_mask])) if np.any(valid_mask) else 0.0
    max_elev = float(np.nanmax(sub[valid_mask])) if np.any(valid_mask) else 1000.0
    print(f"Downsampled grid: {res_x} x {res_z}, elevation range: {min_elev:.1f}m - {max_elev:.1f}m")

    # Header: DTM1 (4s), resX (I), resZ (I), originX (d), originZ (d), width (d), depth (d), minElev (f), maxElev (f)
    origin_x = (min_left + max_right) * 0.5
    origin_z = (min_bottom + max_top) * 0.5
    header = struct.pack('<4sIIddddff', b'DTM1', res_x, res_z, origin_x, origin_z, width_m, depth_m, min_elev, max_elev)

    with open(output_path, 'wb') as f:
        f.write(header)
        sub.tofile(f)

    file_size_mb = os.path.getsize(output_path) / (1024 * 1024)
    print(f"Exported {output_path} successfully ({file_size_mb:.2f} MB).")

if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    output_bin = os.path.join(script_dir, "terrain_15km.bin")
    export_dtm_binary(tiles_dir=script_dir, output_path=output_bin)

