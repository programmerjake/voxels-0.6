/*
 * Voxels is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Voxels is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Voxels; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */
#include "texture/texture_atlas.h"
#include <iostream>
#include <cstdlib>

using namespace std;

namespace
{
Image loadImage()
{
    try
    {
        return Image(L"textures.png");
    }
    catch(exception &e)
    {
        cerr << "error : " << e.what() << endl;
        exit(1);
    }
}
}

const Image TextureAtlas::texture = loadImage();

const TextureAtlas
TextureAtlas::ActivatorRailOff(0, 0, 16, 16),
             TextureAtlas::ActivatorRailOn(16, 0, 16, 16),
             TextureAtlas::BedHead(32, 0, 16, 16),
             TextureAtlas::BedFoot(48, 0, 16, 16),
             TextureAtlas::BedTopSide(64, 0, 16, 16),
             TextureAtlas::BedTopLeftSide(80, 0, 16, 16),
             TextureAtlas::BedTopRightSide(112, 0, 16, 16),
             TextureAtlas::BedBottom(128, 0, 16, 16),
             TextureAtlas::BedBottomLeftSide(96, 0, 16, 16),
             TextureAtlas::BedBottomRightSide(144, 0, 16, 16),
             TextureAtlas::BedBottomSide(160, 0, 16, 16),
             TextureAtlas::BedItem(176, 0, 16, 16),
             TextureAtlas::Font8x8(128, 128, 128, 128),
             TextureAtlas::Shockwave(0, 128, 128, 128),
             TextureAtlas::Bedrock(192, 0, 16, 16),
             TextureAtlas::BirchLeaves(208, 0, 16, 16),
             TextureAtlas::BirchPlank(224, 0, 16, 16),
             TextureAtlas::BirchSapling(240, 0, 16, 16),
             TextureAtlas::BirchWood(0, 16, 16, 16),
             TextureAtlas::WoodEnd(16, 16, 16, 16),
             TextureAtlas::BlazePowder(32, 16, 16, 16),
             TextureAtlas::BlazeRod(48, 16, 16, 16),
             TextureAtlas::Bone(64, 16, 16, 16),
             TextureAtlas::BoneMeal(80, 16, 16, 16),
             TextureAtlas::Bow(96, 16, 16, 16),
             TextureAtlas::BrownMushroom(112, 16, 16, 16),
             TextureAtlas::Bucket(128, 16, 16, 16),
             TextureAtlas::CactusSide(144, 16, 16, 16),
             TextureAtlas::CactusBottom(160, 16, 16, 16),
             TextureAtlas::CactusTop(176, 16, 16, 16),
             TextureAtlas::CactusGreen(192, 16, 16, 16),
             TextureAtlas::ChestSide(208, 16, 16, 16),
             TextureAtlas::ChestTop(224, 16, 16, 16),
             TextureAtlas::ChestFront(240, 16, 16, 16),
             TextureAtlas::CobbleStone(0, 32, 16, 16),
             TextureAtlas::Coal(16, 32, 16, 16),
             TextureAtlas::CoalOre(32, 32, 16, 16),
             TextureAtlas::CocoaSmallSide(48, 32, 16, 16),
             TextureAtlas::CocoaSmallTop(64, 32, 16, 16),
             TextureAtlas::CocoaSmallStem(80, 32, 16, 16),
             TextureAtlas::CocoaMediumSide(96, 32, 16, 16),
             TextureAtlas::CocoaMediumTop(112, 32, 16, 16),
             TextureAtlas::CocoaMediumStem(128, 32, 16, 16),
             TextureAtlas::CocoaLargeSide(144, 32, 16, 16),
             TextureAtlas::CocoaLargeTop(160, 32, 16, 16),
             TextureAtlas::CocoaLargeStem(176, 32, 16, 16),
             TextureAtlas::CocoaBeans(192, 32, 16, 16),
             TextureAtlas::DeadBush(208, 32, 16, 16),
             TextureAtlas::DandelionYellow(224, 32, 16, 16),
             TextureAtlas::Dandelion(240, 32, 16, 16),
             TextureAtlas::CyanDye(0, 48, 16, 16),
             TextureAtlas::Delete0(16, 48, 16, 16),
             TextureAtlas::Delete1(32, 48, 16, 16),
             TextureAtlas::Delete2(48, 48, 16, 16),
             TextureAtlas::Delete3(64, 48, 16, 16),
             TextureAtlas::Delete4(80, 48, 16, 16),
             TextureAtlas::Delete5(96, 48, 16, 16),
             TextureAtlas::Delete6(112, 48, 16, 16),
             TextureAtlas::Delete7(128, 48, 16, 16),
             TextureAtlas::Delete8(144, 48, 16, 16),
             TextureAtlas::Delete9(160, 48, 16, 16),
             TextureAtlas::DetectorRailOff(176, 48, 16, 16),
             TextureAtlas::DetectorRailOn(192, 48, 16, 16),
             TextureAtlas::Diamond(208, 48, 16, 16),
             TextureAtlas::DiamondAxe(224, 48, 16, 16),
             TextureAtlas::DiamondHoe(240, 48, 16, 16),
             TextureAtlas::DiamondOre(0, 64, 16, 16),
             TextureAtlas::DiamondPickaxe(16, 64, 16, 16),
             TextureAtlas::DiamondShovel(32, 64, 16, 16),
             TextureAtlas::Dirt(48, 64, 16, 16),
             TextureAtlas::DirtMask(64, 64, 16, 16),
             TextureAtlas::DispenserSide(80, 64, 16, 16),
             TextureAtlas::DispenserTop(96, 64, 16, 16),
             TextureAtlas::DropperSide(112, 64, 16, 16),
             TextureAtlas::DropperTop(128, 64, 16, 16),
             TextureAtlas::DispenserDropperPistonFurnaceFrame(144, 64, 16, 16),
             TextureAtlas::Emerald(160, 48, 16, 16),
             TextureAtlas::EmeraldOre(176, 64, 16, 16),
             TextureAtlas::PistonBaseSide(192, 64, 16, 16),
             TextureAtlas::PistonBaseTop(208, 64, 16, 16),
             TextureAtlas::PistonHeadSide(224, 64, 16, 16),
             TextureAtlas::PistonHeadFace(240, 64, 16, 16),
             TextureAtlas::PistonHeadBase(0, 80, 16, 16),
             TextureAtlas::StickyPistonHeadFace(16, 80, 16, 16),
             TextureAtlas::FarmlandSide(32, 80, 16, 16),
             TextureAtlas::FarmlandTop(48, 80, 16, 16),
             TextureAtlas::Fire0(64, 80, 16, 16),
             TextureAtlas::Fire1(80, 80, 16, 16),
             TextureAtlas::Fire2(96, 80, 16, 16),
             TextureAtlas::Fire3(112, 80, 16, 16),
             TextureAtlas::Fire4(128, 80, 16, 16),
             TextureAtlas::Fire5(144, 80, 16, 16),
             TextureAtlas::Fire6(160, 80, 16, 16),
             TextureAtlas::Fire7(176, 80, 16, 16),
             TextureAtlas::Flint(192, 80, 16, 16),
             TextureAtlas::FlintAndSteel(208, 80, 16, 16),
             TextureAtlas::FurnaceFrontOff(224, 80, 16, 16),
             TextureAtlas::FurnaceSide(240, 80, 16, 16),
             TextureAtlas::FurnaceFrontOn(0, 96, 16, 16),
             TextureAtlas::WorkBenchTop(16, 96, 16, 16),
             TextureAtlas::WorkBenchSide0(32, 96, 16, 16),
             TextureAtlas::WorkBenchSide1(48, 96, 16, 16),
             TextureAtlas::Glass(64, 96, 16, 16),
             TextureAtlas::GoldAxe(80, 96, 16, 16),
             TextureAtlas::GoldHoe(96, 96, 16, 16),
             TextureAtlas::GoldIngot(112, 96, 16, 16),
             TextureAtlas::GoldOre(128, 96, 16, 16),
             TextureAtlas::GoldPickaxe(144, 96, 16, 16),
             TextureAtlas::GoldShovel(160, 96, 16, 16),
             TextureAtlas::GrassMask(176, 96, 16, 16),
             TextureAtlas::GrassTop(192, 96, 16, 16),
             TextureAtlas::Gravel(208, 96, 16, 16),
             TextureAtlas::GrayDye(224, 96, 16, 16),
             TextureAtlas::Gunpowder(240, 96, 16, 16),
             TextureAtlas::HopperRim(0, 112, 16, 16),
             TextureAtlas::HopperInside(16, 112, 16, 16),
             TextureAtlas::HopperSide(32, 112, 16, 16),
             TextureAtlas::HopperBigBottom(48, 112, 16, 16),
             TextureAtlas::HopperMediumBottom(64 + 4, 112 + 4, 8, 8),
             TextureAtlas::HopperSmallBottom(64, 112, 4, 4),
             TextureAtlas::HopperItem(80, 112, 16, 16),
             TextureAtlas::HotBarBox(256, 236, 20, 20),
             TextureAtlas::InkSac(96, 112, 16, 16),
             TextureAtlas::IronAxe(112, 112, 16, 16),
             TextureAtlas::IronHoe(128, 112, 16, 16),
             TextureAtlas::IronIngot(144, 112, 16, 16),
             TextureAtlas::IronOre(160, 112, 16, 16),
             TextureAtlas::IronPickaxe(176, 112, 16, 16),
             TextureAtlas::IronShovel(192, 112, 16, 16),
             TextureAtlas::JungleLeaves(208, 112, 16, 16),
             TextureAtlas::JunglePlank(224, 112, 16, 16),
             TextureAtlas::JungleSapling(240, 112, 16, 16),
             TextureAtlas::JungleWood(256, 0, 16, 16),
             TextureAtlas::Ladder(256 + 16 * 1, 16 * 0, 16, 16),
             TextureAtlas::LapisLazuli(256 + 16 * 2, 16 * 0, 16, 16),
             TextureAtlas::LapisLazuliOre(256 + 16 * 3, 16 * 0, 16, 16),
             TextureAtlas::WheatItem(256 + 16 * 4, 16 * 0, 16, 16),
             TextureAtlas::LavaBucket(256 + 16 * 5, 16 * 0, 16, 16),
             TextureAtlas::OakLeaves(256 + 16 * 6, 16 * 0, 16, 16),
             TextureAtlas::LeverBaseBigSide(256 + 16 * 7, 16 * 0, 8, 3),
             TextureAtlas::LeverBaseSmallSide(256 + 16 * 7, 16 * 0 + 3, 6, 3),
             TextureAtlas::LeverBaseTop(256 + 16 * 7, 16 * 0 + 6, 8, 6),
             TextureAtlas::LeverHandleSide(256 + 16 * 7 + 8, 16 * 0, 2, 8),
             TextureAtlas::LeverHandleTop(256 + 16 * 7 + 12, 16 * 0, 2, 2),
             TextureAtlas::LeverHandleBottom(256 + 16 * 7 + 10, 16 * 0, 2, 2),
             TextureAtlas::LightBlueDye(384, 0, 16, 16),
             TextureAtlas::LightGrayDye(400, 0, 16, 16),
             TextureAtlas::LimeDye(416, 0, 16, 16),
             TextureAtlas::MagentaDye(432, 0, 16, 16),
             TextureAtlas::MinecartInsideSide(448, 0, 16, 16),
             TextureAtlas::MinecartInsideBottom(464, 0, 16, 16),
             TextureAtlas::MinecartItem(480, 0, 16, 16),
             TextureAtlas::MinecartOutsideLeftRight(496, 0, 16, 16),
             TextureAtlas::MinecartOutsideFrontBack(256, 16, 16, 16),
             TextureAtlas::MinecartOutsideBottom(272, 16, 16, 16),
             TextureAtlas::MinecartOutsideTop(288, 16, 16, 16),
             TextureAtlas::MinecartWithChest(304, 16, 16, 16),
             TextureAtlas::MinecartWithHopper(320, 16, 16, 16),
             TextureAtlas::MinecartWithTNT(336, 16, 16, 16),
             TextureAtlas::MobSpawner(352, 16, 16, 16),
             TextureAtlas::OrangeDye(368, 16, 16, 16),
             TextureAtlas::WaterSide0(384 + 16 * 0, 16, 16, 16),
             TextureAtlas::WaterSide1(384 + 16 * 1, 16, 16, 16),
             TextureAtlas::WaterSide2(384 + 16 * 2, 16, 16, 16),
             TextureAtlas::WaterSide3(384 + 16 * 3, 16, 16, 16),
             TextureAtlas::WaterSide4(384 + 16 * 4, 16, 16, 16),
             TextureAtlas::WaterSide5(384 + 16 * 5, 16, 16, 16),
             TextureAtlas::Obsidian(480, 16, 16, 16),
             TextureAtlas::ParticleSmoke0(256 + 8 * 0, 128, 8, 8),
             TextureAtlas::ParticleSmoke1(256 + 8 * 1, 128, 8, 8),
             TextureAtlas::ParticleSmoke2(256 + 8 * 2, 128, 8, 8),
             TextureAtlas::ParticleSmoke3(256 + 8 * 3, 128, 8, 8),
             TextureAtlas::ParticleSmoke4(256 + 8 * 4, 128, 8, 8),
             TextureAtlas::ParticleSmoke5(256 + 8 * 5, 128, 8, 8),
             TextureAtlas::ParticleSmoke6(256 + 8 * 6, 128, 8, 8),
             TextureAtlas::ParticleSmoke7(256 + 8 * 7, 128, 8, 8),
             TextureAtlas::ParticleFire0(320, 128, 8, 8),
             TextureAtlas::ParticleFire1(328, 128, 8, 8),
             TextureAtlas::PinkDye(496, 16, 16, 16),
             TextureAtlas::PinkStone(256, 16, 16, 16),
             TextureAtlas::PistonShaft(276, 244, 2, 12),
             TextureAtlas::OakPlank(272, 32, 16, 16),
             TextureAtlas::PoweredRailOff(288, 32, 16, 16),
             TextureAtlas::PoweredRailOn(304, 32, 16, 16),
             TextureAtlas::PurpleDye(320, 32, 16, 16),
             TextureAtlas::PurplePortal(336, 32, 16, 16),
             TextureAtlas::Quartz(352, 32, 16, 16),
             TextureAtlas::Rail(368, 32, 16, 16),
             TextureAtlas::RailCurve(384, 32, 16, 16),
             TextureAtlas::RedMushroom(400, 32, 16, 16),
             TextureAtlas::BlockOfRedstone(416, 32, 16, 16),
             TextureAtlas::RedstoneComparatorOff(432, 32, 16, 16),
             TextureAtlas::RedstoneComparatorOn(448, 32, 16, 16),
             TextureAtlas::RedstoneComparatorRepeatorSide(464, 46, 16, 2),
             TextureAtlas::RedstoneShortTorchSideOn(464, 32, 8, 12),
             TextureAtlas::RedstoneShortTorchSideOff(472, 32, 8, 12),
             TextureAtlas::RedstoneDust0(480, 32, 16, 16),
             TextureAtlas::RedstoneDust1(496, 32, 16, 16),
             TextureAtlas::RedstoneDust2Corner(256, 48, 16, 16),
             TextureAtlas::RedstoneDust2Across(272, 48, 16, 16),
             TextureAtlas::RedstoneDust3(288, 48, 16, 16),
             TextureAtlas::RedstoneDust4(304, 48, 16, 16),
             TextureAtlas::RedstoneDustItem(320, 48, 16, 16),
             TextureAtlas::RedstoneRepeatorBarSide(336, 48, 12, 2),
             TextureAtlas::RedstoneRepeatorBarTopBottom(336, 50, 12, 2),
             TextureAtlas::RedstoneRepeatorBarEnd(336, 52, 2, 2),
             TextureAtlas::RedstoneOre(352, 48, 16, 16),
             TextureAtlas::ActiveRedstoneOre(368, 48, 16, 16),
             TextureAtlas::RedstoneRepeatorOff(384, 48, 16, 16),
             TextureAtlas::RedstoneRepeatorOn(400, 48, 16, 16),
             TextureAtlas::RedstoneTorchSideOn(416, 48, 16, 16),
             TextureAtlas::RedstoneTorchSideOff(432, 48, 16, 16),
             TextureAtlas::RedstoneTorchTopOn(423, 54, 2, 2),
             TextureAtlas::RedstoneTorchTopOff(439, 54, 2, 2),
             TextureAtlas::RedstoneTorchBottomOn(423, 62, 2, 2),
             TextureAtlas::RedstoneTorchBottomOff(439, 62, 2, 2),
             TextureAtlas::Rose(448, 48, 16, 16),
             TextureAtlas::RoseRed(464, 48, 16, 16),
             TextureAtlas::Sand(480, 48, 16, 16),
             TextureAtlas::OakSapling(496, 48, 16, 16),
             TextureAtlas::WheatSeeds(256, 64, 16, 16),
             TextureAtlas::Shears(272, 64, 16, 16),
             TextureAtlas::Slime(288, 64, 16, 16),
             TextureAtlas::Snow(304, 64, 16, 16),
             TextureAtlas::SnowBall(320, 64, 16, 16),
             TextureAtlas::SnowGrass(336, 64, 16, 16),
             TextureAtlas::SpruceLeaves(352, 64, 16, 16),
             TextureAtlas::SprucePlank(368, 64, 16, 16),
             TextureAtlas::SpruceSapling(384, 64, 16, 16),
             TextureAtlas::SpruceWood(400, 64, 16, 16),
             TextureAtlas::Stick(416, 64, 16, 16),
             TextureAtlas::Stone(432, 64, 16, 16),
             TextureAtlas::StoneAxe(448, 64, 16, 16),
             TextureAtlas::StoneButtonFace(464, 64, 6, 4),
             TextureAtlas::StoneButtonLongEdge(464, 68, 6, 2),
             TextureAtlas::StoneButtonShortEdge(470, 64, 2, 4),
             TextureAtlas::WoodButtonFace(464, 72, 6, 4),
             TextureAtlas::WoodButtonLongEdge(464, 76, 6, 2),
             TextureAtlas::WoodButtonShortEdge(470, 72, 2, 4),
             TextureAtlas::StoneHoe(480, 64, 16, 16),
             TextureAtlas::StonePickaxe(496, 64, 16, 16),
             TextureAtlas::StonePressurePlateFace(256, 80, 16, 16),
             TextureAtlas::StonePressurePlateSide(272, 80, 16, 2),
             TextureAtlas::StoneShovel(288, 80, 16, 16),
             TextureAtlas::StringItem(304, 80, 16, 16),
             TextureAtlas::TallGrass(320, 80, 16, 16),
             TextureAtlas::LavaSide0(336 + 16 * 0, 80, 16, 16),
             TextureAtlas::LavaSide1(336 + 16 * 1, 80, 16, 16),
             TextureAtlas::LavaSide2(336 + 16 * 2, 80, 16, 16),
             TextureAtlas::LavaSide3(336 + 16 * 3, 80, 16, 16),
             TextureAtlas::LavaSide4(336 + 16 * 4, 80, 16, 16),
             TextureAtlas::LavaSide5(336 + 16 * 5, 80, 16, 16),
             TextureAtlas::TNTSide(432, 80, 16, 16),
             TextureAtlas::TNTTop(448, 80, 16, 16),
             TextureAtlas::TNTBottom(464, 80, 16, 16),
             TextureAtlas::Blank(480, 80, 16, 16),
             TextureAtlas::TorchSide(496, 80, 16, 16),
             TextureAtlas::TorchTop(503, 86, 2, 2),
             TextureAtlas::TorchBottom(503, 94, 2, 2),
             TextureAtlas::Vines(256, 96, 16, 16),
             TextureAtlas::WaterBucket(272, 96, 16, 16),
             TextureAtlas::SpiderWeb(288, 96, 16, 16),
             TextureAtlas::WetFarmland(304, 96, 16, 16),
             TextureAtlas::Wheat0(320 + 16 * 0, 96, 16, 16),
             TextureAtlas::Wheat1(320 + 16 * 1, 96, 16, 16),
             TextureAtlas::Wheat2(320 + 16 * 2, 96, 16, 16),
             TextureAtlas::Wheat3(320 + 16 * 3, 96, 16, 16),
             TextureAtlas::Wheat4(320 + 16 * 4, 96, 16, 16),
             TextureAtlas::Wheat5(320 + 16 * 5, 96, 16, 16),
             TextureAtlas::Wheat6(320 + 16 * 6, 96, 16, 16),
             TextureAtlas::Wheat7(320 + 16 * 7, 96, 16, 16),
             TextureAtlas::OakWood(448, 96, 16, 16),
             TextureAtlas::WoodAxe(464, 96, 16, 16),
             TextureAtlas::WoodHoe(480, 96, 16, 16),
             TextureAtlas::WoodPickaxe(496, 96, 16, 16),
             TextureAtlas::WoodPressurePlateFace(256, 112, 16, 16),
             TextureAtlas::WoodPressurePlateSide(272, 82, 16, 2),
             TextureAtlas::WoodShovel(272, 112, 16, 16),
             TextureAtlas::Wool(288, 112, 16, 16),
             TextureAtlas::Selection(288, 224, 32, 32);
