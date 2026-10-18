// Copyright © 2026 spacegirl65. All Rights Reserved.
//
// Standalone utility to pack individual terrain PBR maps into composite BC5/BC7 DDS textures:
//   1. <prefix>_albedo_height.dds (BC7 sRGB): RGB = Albedo, A = Displacement/Height
//   2. <prefix>_orm.dds           (BC7 Linear): R = Ambient Occlusion, G = Roughness, B = 0, A = 255
//   3. <prefix>_normal.dds        (BC5 UNORM): Already in BC5 format!

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static bool RunCommand(const std::string& cmd)
{
    // Wrap entire command in extra quotes for Windows cmd.exe multi-argument quote handling
    const std::string wrappedCmd = "\"" + cmd + "\"";
    const int ret = std::system(wrappedCmd.c_str());
    return (ret == 0);
}

static size_t GetDdsHeaderSize(const std::vector<char>& bytes)
{
    if (bytes.size() >= 148 &&
        bytes[84] == 'D' && bytes[85] == 'X' && bytes[86] == '1' && bytes[87] == '0')
    {
        return 148;
    }
    return 128;
}

int main(int argc, char* argv[])
{
    const fs::path rootDir = (argc > 1) ? fs::path(argv[1]) : fs::current_path();
    const fs::path texDir = rootDir / "resources" / "environment" / "terrain" / "textures";
    const fs::path texconv = rootDir / "texconv.exe";
    const fs::path tempDir = rootDir / "intermediate" / "texture_packing";

    std::cout << "========================================================\n";
    std::cout << "  Sandbox3D - Composite BC5/BC7 Terrain Texture Packer  \n";
    std::cout << "========================================================\n\n";

    if (!fs::exists(texconv))
    {
        std::cerr << "Error: texconv.exe not found at: " << texconv << "\n";
        return 1;
    }

    if (!fs::exists(texDir))
    {
        std::cerr << "Error: Textures directory not found at: " << texDir << "\n";
        return 1;
    }

    fs::create_directories(tempDir);

    struct BiomeEntry
    {
        std::string srcPrefix;
        std::string dstPrefix;
    };

    const std::vector<BiomeEntry> biomes = {
        { "uncut_grass_oilpt20_4k",  "uncut_grass_1" }, // Meadow
        { "wild_grass_umjlabus_4k",  "wild_grass_1"  }, // MidSlope
        { "wild_grass_vbslfeqfw_4k", "wild_grass_2"  }, // Plateau
        { "grass_dried_olqkj0_4k",   "grass_dried_1" }, // DryThatch
        { "rock_cliff_vl3ibcxlw_4k", "rock_cliff_1"  }, // RockCliff
        { "uncut_grass_pftph0_4k",   "uncut_grass_2" }, // AltMeadow
        { "wild_grass_umrmdgps_4k",  "wild_grass_3"  }, // AltMidSlope
        { "wild_grass_vbikagyn_4k",  "wild_grass_4"  }  // AltPlateau
    };

    for (size_t idx = 0; idx < biomes.size(); ++idx)
    {
        const std::string& srcPrefix = biomes[idx].srcPrefix;
        const std::string& dstPrefix = biomes[idx].dstPrefix;
        std::cout << "--------------------------------------------------------\n";
        std::cout << "[" << (idx + 1) << "/" << biomes.size() << "] Biome: " << srcPrefix << " -> " << dstPrefix << "\n";
        std::cout << "--------------------------------------------------------\n";

        // ---------------------------------------------------------------------
        // 0. Ensure Normal map is present with clean destination name
        // ---------------------------------------------------------------------
        const fs::path targetNormal = texDir / (dstPrefix + "_normal.dds");
        const fs::path srcNormal    = texDir / (srcPrefix + "_normal.dds");
        if (!fs::exists(targetNormal) && fs::exists(srcNormal))
        {
            fs::copy_file(srcNormal, targetNormal);
            std::cout << "  ==> Copied Normal map: " << targetNormal.filename().string() << "\n";
        }

        // ---------------------------------------------------------------------
        // 1. Pack Albedo + Height (Displacement) -> _albedo_height.dds (BC7 sRGB)
        // ---------------------------------------------------------------------
        const fs::path targetAlbHeight = texDir / (dstPrefix + "_albedo_height.dds");
        if (fs::exists(targetAlbHeight) && fs::file_size(targetAlbHeight) > 1024)
        {
            std::cout << "  [Skip] Albedo+Height already exists: " << targetAlbHeight.filename().string() << "\n";
        }
        else
        {
            const fs::path srcAlbedo = texDir / (srcPrefix + "_albedo.dds");
            const fs::path srcDisp   = texDir / (srcPrefix + "_displacement.dds");

            if (!fs::exists(srcAlbedo) || !fs::exists(srcDisp))
            {
                std::cerr << "  Error: Missing input file: " << srcAlbedo << " or " << srcDisp << "\n";
                continue;
            }

            std::cout << "  -> Decompressing Albedo (sRGB) & Displacement (Linear)...\n";
            std::string cmdDecompAlb = "\"" + texconv.string() + "\" -m 1 -f R8G8B8A8_UNORM_SRGB -y -o \"" + tempDir.string() + "\" \"" + srcAlbedo.string() + "\"";
            std::string cmdDecompDsp = "\"" + texconv.string() + "\" -m 1 -f R8G8B8A8_UNORM -y -o \"" + tempDir.string() + "\" \"" + srcDisp.string() + "\"";
            RunCommand(cmdDecompAlb);
            RunCommand(cmdDecompDsp);

            const fs::path tempAlb = tempDir / (srcPrefix + "_albedo.dds");
            const fs::path tempDsp = tempDir / (srcPrefix + "_displacement.dds");

            std::ifstream fileAlb(tempAlb, std::ios::binary);
            std::ifstream fileDsp(tempDsp, std::ios::binary);

            if (!fileAlb || !fileDsp)
            {
                std::cerr << "  Error: Failed to read decompressed files for " << srcPrefix << "\n";
                continue;
            }

            std::vector<char> albBytes((std::istreambuf_iterator<char>(fileAlb)), std::istreambuf_iterator<char>());
            std::vector<char> dspBytes((std::istreambuf_iterator<char>(fileDsp)), std::istreambuf_iterator<char>());
            fileAlb.close();
            fileDsp.close();

            const size_t albHeaderSize = GetDdsHeaderSize(albBytes);
            const size_t dspHeaderSize = GetDdsHeaderSize(dspBytes);

            if (albBytes.size() < albHeaderSize || dspBytes.size() < dspHeaderSize ||
                (albBytes.size() - albHeaderSize) != (dspBytes.size() - dspHeaderSize))
            {
                std::cerr << "  Error: Payload size mismatch between Albedo (" << (albBytes.size() - albHeaderSize)
                          << ") and Displacement (" << (dspBytes.size() - dspHeaderSize) << ")\n";
                continue;
            }

            const size_t pixelCount = (albBytes.size() - albHeaderSize) / 4;
            auto* pAlb = reinterpret_cast<uint8_t*>(albBytes.data() + albHeaderSize);
            const auto* pDsp = reinterpret_cast<const uint8_t*>(dspBytes.data() + dspHeaderSize);

            // Interleave: Copy displacement red channel into albedo alpha channel
            for (size_t i = 0; i < pixelCount; ++i)
            {
                pAlb[i * 4 + 3] = pDsp[i * 4 + 0]; // Alpha = Displacement Red
            }

            const fs::path rawOut = tempDir / (dstPrefix + "_albedo_height_raw.dds");
            std::ofstream fileOut(rawOut, std::ios::binary);
            fileOut.write(albBytes.data(), albBytes.size());
            fileOut.close();

            std::cout << "  -> Compressing to BC7_UNORM_SRGB (-sepalpha) with GPU...\n";
            std::string cmdCompress = "\"" + texconv.string() + "\" -f BC7_UNORM_SRGB -sepalpha -m 13 -gpu 1 -y -o \"" + texDir.string() + "\" \"" + rawOut.string() + "\"";
            RunCommand(cmdCompress);

            const fs::path generatedDds = texDir / (dstPrefix + "_albedo_height_raw.dds");
            if (fs::exists(generatedDds))
            {
                if (fs::exists(targetAlbHeight))
                {
                    fs::remove(targetAlbHeight);
                }
                fs::rename(generatedDds, targetAlbHeight);
                std::cout << "  ==> Created: " << targetAlbHeight.filename().string()
                          << " (" << (fs::file_size(targetAlbHeight) / (1024 * 1024)) << " MB)\n";
            }
            else
            {
                std::cerr << "  Error: Failed to encode " << targetAlbHeight << "\n";
            }

            fs::remove(tempAlb);
            fs::remove(tempDsp);
            fs::remove(rawOut);
        }

        // ---------------------------------------------------------------------
        // 2. Pack AO + Roughness -> _orm.dds (BC7 Linear)
        // ---------------------------------------------------------------------
        const fs::path targetOrm = texDir / (dstPrefix + "_orm.dds");
        if (fs::exists(targetOrm) && fs::file_size(targetOrm) > 1024)
        {
            std::cout << "  [Skip] ORM already exists: " << targetOrm.filename().string() << "\n";
        }
        else
        {
            const fs::path srcAo    = texDir / (srcPrefix + "_ao.dds");
            const fs::path srcRough = texDir / (srcPrefix + "_roughness.dds");

            if (!fs::exists(srcAo) || !fs::exists(srcRough))
            {
                std::cerr << "  Error: Missing input file: " << srcAo << " or " << srcRough << "\n";
                continue;
            }

            std::cout << "  -> Decompressing AO & Roughness...\n";
            std::string cmdDecompAo  = "\"" + texconv.string() + "\" -m 1 -f R8G8B8A8_UNORM -y -o \"" + tempDir.string() + "\" \"" + srcAo.string() + "\"";
            std::string cmdDecompRgh = "\"" + texconv.string() + "\" -m 1 -f R8G8B8A8_UNORM -y -o \"" + tempDir.string() + "\" \"" + srcRough.string() + "\"";
            RunCommand(cmdDecompAo);
            RunCommand(cmdDecompRgh);

            const fs::path tempAo  = tempDir / (srcPrefix + "_ao.dds");
            const fs::path tempRgh = tempDir / (srcPrefix + "_roughness.dds");

            std::ifstream fileAo(tempAo, std::ios::binary);
            std::ifstream fileRgh(tempRgh, std::ios::binary);

            if (!fileAo || !fileRgh)
            {
                std::cerr << "  Error: Failed to read decompressed files for " << srcPrefix << "\n";
                continue;
            }

            std::vector<char> aoBytes((std::istreambuf_iterator<char>(fileAo)), std::istreambuf_iterator<char>());
            std::vector<char> rghBytes((std::istreambuf_iterator<char>(fileRgh)), std::istreambuf_iterator<char>());
            fileAo.close();
            fileRgh.close();

            if (aoBytes.size() != rghBytes.size() || aoBytes.size() < 128)
            {
                std::cerr << "  Error: Size mismatch between AO (" << aoBytes.size() << ") and Roughness (" << rghBytes.size() << ")\n";
                continue;
            }

            const size_t headerSize = 128;
            const size_t pixelCount = (aoBytes.size() - headerSize) / 4;
            auto* pAo = reinterpret_cast<uint8_t*>(aoBytes.data() + headerSize);
            const auto* pRgh = reinterpret_cast<const uint8_t*>(rghBytes.data() + headerSize);

            // Interleave: R = AO, G = Roughness, B = 0 (Metallic), A = 255
            for (size_t i = 0; i < pixelCount; ++i)
            {
                pAo[i * 4 + 0] = pAo[i * 4 + 0];  // R = AO
                pAo[i * 4 + 1] = pRgh[i * 4 + 0]; // G = Roughness
                pAo[i * 4 + 2] = 0;               // B = Metallic / unused (0)
                pAo[i * 4 + 3] = 255;             // A = Opaque (255)
            }

            const fs::path rawOut = tempDir / (dstPrefix + "_orm_raw.dds");
            std::ofstream fileOut(rawOut, std::ios::binary);
            fileOut.write(aoBytes.data(), aoBytes.size());
            fileOut.close();

            std::cout << "  -> Compressing to BC7_UNORM (Linear) with GPU...\n";
            std::string cmdCompress = "\"" + texconv.string() + "\" -f BC7_UNORM -m 13 -gpu 1 -y -o \"" + texDir.string() + "\" \"" + rawOut.string() + "\"";
            RunCommand(cmdCompress);

            const fs::path generatedDds = texDir / (dstPrefix + "_orm_raw.dds");
            if (fs::exists(generatedDds))
            {
                fs::rename(generatedDds, targetOrm);
                std::cout << "  ==> Created: " << targetOrm.filename().string()
                          << " (" << (fs::file_size(targetOrm) / (1024 * 1024)) << " MB)\n";
            }
            else
            {
                std::cerr << "  Error: Failed to encode " << targetOrm << "\n";
            }

            fs::remove(tempAo);
            fs::remove(tempRgh);
            fs::remove(rawOut);
        }

        std::cout << "\n";
    }

    fs::remove_all(tempDir);
    std::cout << "========================================================\n";
    std::cout << ">>> All 8 Biomes Packed Successfully into Composite BC5/BC7 Formats! <<<\n";
    std::cout << "========================================================\n";
    return 0;
}

