//
// LICENSE:
//
// Copyright (c) 2016 -- 2020 Fabio Pellacini
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

#include <yocto/yocto_commonio.h>
#include <yocto/yocto_image.h>
#include <yocto/yocto_math.h>
#include <yocto/yocto_sceneio.h>
#include <yocto/yocto_trace.h>
#include <yocto_particle/yocto_particle.h>
using namespace yocto::math;
namespace sio = yocto::sceneio;
namespace img = yocto::image;
namespace cli = yocto::commonio;
namespace trc = yocto::trace;
namespace par = yocto::particle;

#include <map>
#include <memory>
using namespace std::string_literals;

#include "ext/filesystem.hpp"
namespace sfs = ghc::filesystem;

// construct a scene from io
void init_scene(trc::scene* scene, sio::model* ioscene, trc::camera*& camera,
    sio::camera* iocamera, sio::progress_callback progress_cb = {}) {
  // handle progress
  auto progress = vec2i{
      0, (int)ioscene->cameras.size() + (int)ioscene->environments.size() +
             (int)ioscene->materials.size() + (int)ioscene->textures.size() +
             (int)ioscene->shapes.size() + (int)ioscene->subdivs.size() +
             (int)ioscene->instances.size() + (int)ioscene->objects.size()};

  auto camera_map     = std::unordered_map<sio::camera*, trc::camera*>{};
  camera_map[nullptr] = nullptr;
  for (auto iocamera : ioscene->cameras) {
    if (progress_cb) progress_cb("convert camera", progress.x++, progress.y);
    auto camera = add_camera(scene);
    set_frame(camera, iocamera->frame);
    set_lens(camera, iocamera->lens, iocamera->aspect, iocamera->film,
        iocamera->orthographic);
    set_focus(camera, iocamera->aperture, iocamera->focus);
    camera_map[iocamera] = camera;
  }

  auto texture_map     = std::unordered_map<sio::texture*, trc::texture*>{};
  texture_map[nullptr] = nullptr;
  for (auto iotexture : ioscene->textures) {
    if (progress_cb) progress_cb("convert texture", progress.x++, progress.y);
    auto texture = add_texture(scene);
    if (!iotexture->colorf.empty()) {
      set_texture(texture, iotexture->colorf);
    } else if (!iotexture->colorb.empty()) {
      set_texture(texture, iotexture->colorb);
    } else if (!iotexture->scalarf.empty()) {
      set_texture(texture, iotexture->scalarf);
    } else if (!iotexture->scalarb.empty()) {
      set_texture(texture, iotexture->scalarb);
    }
    texture_map[iotexture] = texture;
  }

  auto material_map     = std::unordered_map<sio::material*, trc::material*>{};
  material_map[nullptr] = nullptr;
  for (auto iomaterial : ioscene->materials) {
    if (progress_cb) progress_cb("convert material", progress.x++, progress.y);
    auto material = add_material(scene);
    set_emission(material, iomaterial->emission,
        texture_map.at(iomaterial->emission_tex));
    set_color(
        material, iomaterial->color, texture_map.at(iomaterial->color_tex));
    set_specular(material, iomaterial->specular,
        texture_map.at(iomaterial->specular_tex));
    set_ior(material, iomaterial->ior);
    set_metallic(material, iomaterial->metallic,
        texture_map.at(iomaterial->metallic_tex));
    set_transmission(material, iomaterial->transmission, iomaterial->thin,
        iomaterial->trdepth, texture_map.at(iomaterial->transmission_tex));
    set_translucency(material, iomaterial->translucency, iomaterial->thin,
        iomaterial->trdepth, texture_map.at(iomaterial->translucency_tex));
    set_roughness(material, iomaterial->roughness,
        texture_map.at(iomaterial->roughness_tex));
    set_opacity(
        material, iomaterial->opacity, texture_map.at(iomaterial->opacity_tex));
    set_thin(material, iomaterial->thin);
    set_normalmap(material, texture_map.at(iomaterial->normal_tex));
    set_scattering(material, iomaterial->scattering, iomaterial->scanisotropy,
        texture_map.at(iomaterial->scattering_tex));
    material_map[iomaterial] = material;
  }

  for (auto iosubdiv : ioscene->subdivs) {
    if (progress_cb) progress_cb("convert subdiv", progress.x++, progress.y);
    tesselate_subdiv(ioscene, iosubdiv);
  }

  auto shape_map     = std::unordered_map<sio::shape*, trc::shape*>{};
  shape_map[nullptr] = nullptr;
  for (auto ioshape : ioscene->shapes) {
    if (progress_cb) progress_cb("convert shape", progress.x++, progress.y);
    auto shape = add_shape(scene);
    set_points(shape, ioshape->points);
    set_lines(shape, ioshape->lines);
    set_triangles(shape, ioshape->triangles);
    set_quads(shape, ioshape->quads);
    set_positions(shape, ioshape->positions);
    set_normals(shape, ioshape->normals);
    set_texcoords(shape, ioshape->texcoords);
    set_colors(shape, ioshape->colors);
    set_radius(shape, ioshape->radius);
    set_tangents(shape, ioshape->tangents);
    shape_map[ioshape] = shape;
  }

  auto instance_map     = std::unordered_map<sio::instance*, trc::instance*>{};
  instance_map[nullptr] = nullptr;
  for (auto ioinstance : ioscene->instances) {
    if (progress_cb) progress_cb("convert instance", progress.x++, progress.y);
    auto instance = add_instance(scene);
    set_frames(instance, ioinstance->frames);
    instance_map[ioinstance] = instance;
  }

  for (auto ioobject : ioscene->objects) {
    if (progress_cb) progress_cb("convert object", progress.x++, progress.y);
    auto object = add_object(scene);
    set_frame(object, ioobject->frame);
    set_shape(object, shape_map.at(ioobject->shape));
    set_material(object, material_map.at(ioobject->material));
    set_instance(object, instance_map.at(ioobject->instance));
  }

  for (auto ioenvironment : ioscene->environments) {
    if (progress_cb)
      progress_cb("convert environment", progress.x++, progress.y);
    auto environment = add_environment(scene);
    set_frame(environment, ioenvironment->frame);
    set_emission(environment, ioenvironment->emission,
        texture_map.at(ioenvironment->emission_tex));
  }

  // done
  if (progress_cb) progress_cb("convert done", progress.x++, progress.y);

  // get camera
  camera = camera_map.at(iocamera);
}

void flatten_scene(sio::model* ioscene) {
  for (auto ioobject : ioscene->objects) {
    for (auto& position : ioobject->shape->positions)
      position = transform_point(ioobject->frame, position);
    for (auto& normal : ioobject->shape->normals)
      normal = transform_normal(ioobject->frame, normal);
    ioobject->frame = identity3x4f;
  }
}

void init_ptscene(par::scene* ptscene, sio::model* ioscene,
    std::unordered_map<sio::shape*, par::shape*>& ptshapemap,
    sio::progress_callback                        progress_cb) {
  // handle progress
  auto progress = vec2i{0, (int)ioscene->objects.size()};

  // shapes
  static auto velocity = std::unordered_map<std::string, float>{
      {"floor", 0}, {"particles", 1}, {"cloth", 0}, {"collider", 0}};
  for (auto ioobject : ioscene->objects) {
    if (progress_cb) progress_cb("convert object", progress.x++, progress.y);
    auto ioshape    = ioobject->shape;
    auto iomaterial = ioobject->material;
    if (iomaterial->name == "particles") {
      auto ptshape = add_particles(
          ptscene, ioshape->points, ioshape->positions, ioshape->radius, 1, 1);
      ptshapemap[ioshape] = ptshape;
    } else if (ioobject->material->name == "cloth") {
      auto nverts  = (int)ioshape->positions.size();
      auto ptshape = add_cloth(ptscene, ioshape->quads, ioshape->positions,
          ioshape->normals, ioshape->radius, 0.5, 1/8000.0,
          {nverts - 1, nverts - (int)sqrt(nverts)});
      ptshapemap[ioshape] = ptshape;
    } else if (ioobject->material->name == "collider") {
      add_collider(ptscene, ioshape->triangles, ioshape->quads,
          ioshape->positions, ioshape->normals, ioshape->radius);
    } else if (ioobject->material->name == "floor") {
      add_collider(ptscene, ioshape->triangles, ioshape->quads,
          ioshape->positions, ioshape->normals, ioshape->radius);
    } else {
      cli::print_fatal("unknown material " + ioobject->material->name);
    }
  }

  // done
  if (progress_cb) progress_cb("convert done", progress.x++, progress.y);
}

void update_ioscene(
    const std::unordered_map<sio::shape*, par::shape*>& ptshapemap) {
  for (auto [ioshape, ptshape] : ptshapemap) {
    get_positions(ptshape, ioshape->positions);
    get_normals(ptshape, ioshape->normals);
  }
}

int main(int argc, const char* argv[]) {
  // options
  auto ptparams    = par::simulation_params{};
  auto trparams    = trc::trace_params{};
  auto camera_name = ""s;
  auto imfilename  = "out.hdr"s;
  auto filename    = "scene.json"s;

  // parse command line
  auto cli = cli::make_cli("yscntrace", "Offline path tracing");
  add_option(cli, "--camera", camera_name, "Camera name.");
  add_option(cli, "--solver", ptparams.solver, "Solver", par::solver_names);
  add_option(cli, "--frames", ptparams.frames, "Simulation frames.");
  add_option(cli, "--resolution", trparams.resolution, "Image resolution.");
  add_option(cli, "--samples", trparams.samples, "Number of samples.");
  add_option(
      cli, "--tracer", trparams.sampler, "Trace type.", trc::sampler_names);
  add_option(cli, "--output-image,-o", imfilename, "Image filename");
  add_option(cli, "scene", filename, "Scene filename", true);
  parse_cli(cli, argc, argv);

  // scene loading
  auto ioscene_guard = std::make_unique<sio::model>();
  auto ioscene       = ioscene_guard.get();
  auto ioerror       = ""s;
  if (!load_scene(filename, ioscene, ioerror, cli::print_progress))
    cli::print_fatal(ioerror);
  flatten_scene(ioscene);

  // simulate
  auto ptscene_guard = std::make_unique<par::scene>();
  auto ptscene       = ptscene_guard.get();
  auto ptshapemap    = std::unordered_map<sio::shape*, par::shape*>{};
  init_ptscene(ptscene, ioscene, ptshapemap, cli::print_progress);

  // simulate
  simulate_frames(ptscene, ptparams, cli::print_progress);

  // update scene
  update_ioscene(ptshapemap);

  // get camera
  auto iocamera = get_camera(ioscene, camera_name);

  // convert scene
  auto scene_guard = std::make_unique<trc::scene>();
  auto scene       = scene_guard.get();
  auto camera      = (trc::camera*)nullptr;
  init_scene(scene, ioscene, camera, iocamera, cli::print_progress);

  // cleanup
  if (ioscene_guard) ioscene_guard.reset();
  if (ptscene_guard) ptscene_guard.reset();

  // build bvh
  init_bvh(scene, trparams, cli::print_progress);

  // init renderer
  init_lights(scene, cli::print_progress);

  // fix renderer type if no lights
  if (scene->lights.empty() && is_sampler_lit(trparams)) {
    cli::print_info("no lights presents, switching to eyelight shader");
    trparams.sampler = trc::sampler_type::eyelight;
  }

  // render
  auto render = trc::trace_image(
      scene, camera, trparams, cli::print_progress, {});

  // save image
  cli::print_progress("save image", 0, 1);
  if (!save_image(imfilename, render, ioerror)) cli::print_fatal(ioerror);
  cli::print_progress("save image", 1, 1);

  // done
  return 0;
}
