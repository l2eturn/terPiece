#define CGLTF_IMPLEMENTATION
#include "scene/gltf.h"
#include <cgltf.h>
#include <meshoptimizer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <nlohmann/json.hpp>
#include "gfx/texture.h"

namespace {

// EXT_meshopt_compression: decode every compressed buffer view into memory cgltf reads from
bool decodeMeshopt(cgltf_data* data) {
    for (cgltf_size i = 0; i < data->buffer_views_count; ++i) {
        cgltf_buffer_view& v = data->buffer_views[i];
        if (!v.has_meshopt_compression) continue;
        cgltf_meshopt_compression& mc = v.meshopt_compression;
        const unsigned char* src = (const unsigned char*)mc.buffer->data + mc.offset;
        void* dst = std::malloc(mc.count * mc.stride);
        int rc = -1;
        switch (mc.mode) {
            case cgltf_meshopt_compression_mode_attributes:
                rc = meshopt_decodeVertexBuffer(dst, mc.count, mc.stride, src, mc.size);
                break;
            case cgltf_meshopt_compression_mode_triangles:
                rc = meshopt_decodeIndexBuffer(dst, mc.count, mc.stride, src, mc.size);
                break;
            case cgltf_meshopt_compression_mode_indices:
                rc = meshopt_decodeIndexSequence(dst, mc.count, mc.stride, src, mc.size);
                break;
            default:
                break;
        }
        if (rc != 0) {
            std::free(dst);
            return false;
        }
        switch (mc.filter) {
            case cgltf_meshopt_compression_filter_octahedral:
                meshopt_decodeFilterOct(dst, mc.count, mc.stride);
                break;
            case cgltf_meshopt_compression_filter_quaternion:
                meshopt_decodeFilterQuat(dst, mc.count, mc.stride);
                break;
            case cgltf_meshopt_compression_filter_exponential:
                meshopt_decodeFilterExp(dst, mc.count, mc.stride);
                break;
            default:
                break;
        }
        // cgltf frees `data` in cgltf_free
        v.data = dst;
    }
    return true;
}

struct Loader {
    cgltf_data* data = nullptr;
    std::string dir;
    std::map<const cgltf_image*, std::shared_ptr<Texture>> textures;
    std::map<const cgltf_material*, MatPtr> materials;
    std::map<const cgltf_node*, Object3D*> nodes;

    // EXT_texture_webp keeps its image index in an unparsed extension
    const cgltf_image* textureImage(const cgltf_texture* t) {
        if (!t) return nullptr;
        if (t->image) return t->image;
        for (cgltf_size i = 0; i < t->extensions_count; ++i) {
            if (std::strcmp(t->extensions[i].name, "EXT_texture_webp") != 0) continue;
            auto j = nlohmann::json::parse(t->extensions[i].data, nullptr, false);
            if (j.is_object() && j.contains("source")) {
                int s = j["source"];
                if (s >= 0 && (cgltf_size)s < data->images_count) return &data->images[s];
            }
        }
        return nullptr;
    }

    std::shared_ptr<Texture> texture(const cgltf_texture* t, bool srgb) {
        const cgltf_image* img = textureImage(t);
        if (!img) return nullptr;
        auto it = textures.find(img);
        if (it != textures.end()) return it->second;
        int w = 0, h = 0;
        std::vector<uint8_t> px;
        bool ok = false;
        if (img->buffer_view) {
            const uint8_t* p = (const uint8_t*)cgltf_buffer_view_data(img->buffer_view);
            ok = p && decodeImageMemory(p, img->buffer_view->size, w, h, px);
        } else if (img->uri) {
            ok = decodeImageFile(dir + "/" + img->uri, w, h, px);
        }
        std::shared_ptr<Texture> tex;
        if (ok) {
            tex = std::make_shared<Texture>();
            TextureOptions o;
            o.srgb = srgb;
            o.nearest = false;
            o.flipY = false;
            tex->upload(w, h, px, o);
        }
        textures[img] = tex;
        return tex;
    }

    MatPtr material(const cgltf_material* m, bool vertexColors) {
        if (!m) {
            auto d = standard(glm::vec3(1), 1, 1);
            d->vertexColors = vertexColors;
            return d;
        }
        auto key = materials.find(m);
        if (key != materials.end() && key->second->vertexColors == vertexColors) return key->second;
        auto mat = std::make_shared<Material>();
        mat->type = MaterialType::Standard;
        mat->vertexColors = vertexColors;
        if (m->has_pbr_metallic_roughness) {
            auto& p = m->pbr_metallic_roughness;
            mat->color = {p.base_color_factor[0], p.base_color_factor[1], p.base_color_factor[2]};
            mat->opacity = p.base_color_factor[3];
            mat->metalness = p.metallic_factor;
            mat->roughness = p.roughness_factor;
            mat->map = texture(p.base_color_texture.texture, true);
        }
        mat->emissive = {m->emissive_factor[0], m->emissive_factor[1], m->emissive_factor[2]};
        if (m->has_emissive_strength) mat->emissiveIntensity = m->emissive_strength.emissive_strength;
        mat->emissiveMap = texture(m->emissive_texture.texture, true);
        if (m->alpha_mode == cgltf_alpha_mode_blend) {
            mat->transparent = true;
            mat->depthWrite = false;
        } else if (m->alpha_mode == cgltf_alpha_mode_mask) {
            mat->alphaTest = m->alpha_cutoff;
        }
        if (m->double_sided) mat->side = Side::Double;
        materialNames[mat.get()] = m->name ? m->name : "";
        materials[m] = mat;
        return mat;
    }
    std::map<Material*, std::string> materialNames;

    GeoPtr primitive(const cgltf_primitive& p) {
        auto g = std::make_shared<Geometry>();
        for (cgltf_size a = 0; a < p.attributes_count; ++a) {
            const cgltf_attribute& at = p.attributes[a];
            const cgltf_accessor* acc = at.data;
            size_t n = acc->count;
            if (at.type == cgltf_attribute_type_position) {
                g->position.resize(n);
                for (size_t i = 0; i < n; ++i) cgltf_accessor_read_float(acc, i, &g->position[i].x, 3);
            } else if (at.type == cgltf_attribute_type_normal) {
                g->normal.resize(n);
                for (size_t i = 0; i < n; ++i) cgltf_accessor_read_float(acc, i, &g->normal[i].x, 3);
            } else if (at.type == cgltf_attribute_type_texcoord && at.index == 0) {
                g->uv.resize(n);
                for (size_t i = 0; i < n; ++i) cgltf_accessor_read_float(acc, i, &g->uv[i].x, 2);
            } else if (at.type == cgltf_attribute_type_color && at.index == 0) {
                g->color.resize(n);
                int comps = (int)cgltf_num_components(acc->type);
                for (size_t i = 0; i < n; ++i) {
                    float c[4] = {1, 1, 1, 1};
                    cgltf_accessor_read_float(acc, i, c, comps);
                    g->color[i] = {c[0], c[1], c[2]};
                }
            }
        }
        if (p.indices) {
            g->index.resize(p.indices->count);
            for (size_t i = 0; i < p.indices->count; ++i) g->index[i] = (uint32_t)cgltf_accessor_read_index(p.indices, i);
        }
        if (g->normal.size() != g->position.size()) g->computeVertexNormals();
        return g;
    }

    ObjPtr node(const cgltf_node* n) {
        auto o = makeGroup(n->name ? n->name : "");
        if (n->has_matrix) {
            glm::mat4 m;
            std::memcpy(&m[0][0], n->matrix, sizeof(float) * 16);
            o->position = glm::vec3(m[3]);
            glm::vec3 s(glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2])));
            o->scale = s;
            glm::mat3 r(glm::vec3(m[0]) / s.x, glm::vec3(m[1]) / s.y, glm::vec3(m[2]) / s.z);
            o->setQuaternion(glm::quat_cast(r));
        } else {
            if (n->has_translation) o->position = {n->translation[0], n->translation[1], n->translation[2]};
            if (n->has_rotation) o->setQuaternion(glm::quat(n->rotation[3], n->rotation[0], n->rotation[1], n->rotation[2]));
            if (n->has_scale) o->scale = {n->scale[0], n->scale[1], n->scale[2]};
        }
        if (n->mesh) {
            // like GLTFLoader: one primitive -> the node itself is the mesh; several -> child meshes
            const cgltf_mesh& mesh = *n->mesh;
            for (cgltf_size i = 0; i < mesh.primitives_count; ++i) {
                const cgltf_primitive& p = mesh.primitives[i];
                if (p.type != cgltf_primitive_type_triangles) continue;
                bool vc = false;
                for (cgltf_size a = 0; a < p.attributes_count; ++a)
                    if (p.attributes[a].type == cgltf_attribute_type_color) vc = true;
                auto geo = primitive(p);
                auto mat = material(p.material, vc);
                if (mesh.primitives_count == 1) {
                    o->kind = ObjectKind::Mesh;
                    o->geometry = geo;
                    o->materials = {mat};
                } else {
                    auto child = makeMesh(geo, mat);
                    child->name = o->name;
                    o->add(child);
                }
            }
        }
        nodes[n] = o.get();
        for (cgltf_size i = 0; i < n->children_count; ++i) o->add(node(n->children[i]));
        return o;
    }
};

}  // namespace

// material names travel with the materials (home.cpp swaps materials by name)
std::map<const Material*, std::string>& gltfMaterialNames() {
    static std::map<const Material*, std::string> m;
    return m;
}

std::optional<GltfScene> loadGltf(const std::string& path) {
    cgltf_options opts{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&opts, path.c_str(), &data) != cgltf_result_success) {
        std::fprintf(stderr, "gltf: cannot parse %s\n", path.c_str());
        return std::nullopt;
    }
    if (cgltf_load_buffers(&opts, data, path.c_str()) != cgltf_result_success || !decodeMeshopt(data)) {
        std::fprintf(stderr, "gltf: cannot load buffers of %s\n", path.c_str());
        cgltf_free(data);
        return std::nullopt;
    }
    Loader L;
    L.data = data;
    L.dir = path.substr(0, path.find_last_of('/'));
    GltfScene out;
    out.root = makeGroup("gltf");
    const cgltf_scene* sc = data->scene ? data->scene : (data->scenes_count ? &data->scenes[0] : nullptr);
    if (sc) {
        for (cgltf_size i = 0; i < sc->nodes_count; ++i) out.root->add(L.node(sc->nodes[i]));
    } else {
        for (cgltf_size i = 0; i < data->nodes_count; ++i)
            if (!data->nodes[i].parent) out.root->add(L.node(&data->nodes[i]));
    }
    for (auto& [m, n] : L.materialNames) gltfMaterialNames()[m] = n;

    for (cgltf_size a = 0; a < data->animations_count; ++a) {
        const cgltf_animation& an = data->animations[a];
        GltfAnimation clip;
        clip.name = an.name ? an.name : "";
        for (cgltf_size c = 0; c < an.channels_count; ++c) {
            const cgltf_animation_channel& ch = an.channels[c];
            auto it = L.nodes.find(ch.target_node);
            if (it == L.nodes.end()) continue;
            int path = ch.target_path == cgltf_animation_path_type_translation ? 0
                       : ch.target_path == cgltf_animation_path_type_rotation  ? 1
                       : ch.target_path == cgltf_animation_path_type_scale     ? 2
                                                                               : -1;
            if (path < 0) continue;
            GltfAnimation::Channel out;
            out.node = it->second;
            out.path = path;
            out.interp = ch.sampler->interpolation == cgltf_interpolation_type_step           ? 1
                         : ch.sampler->interpolation == cgltf_interpolation_type_cubic_spline ? 2
                                                                                               : 0;
            const cgltf_accessor* in = ch.sampler->input;
            const cgltf_accessor* val = ch.sampler->output;
            out.times.resize(in->count);
            for (size_t i = 0; i < in->count; ++i) cgltf_accessor_read_float(in, i, &out.times[i], 1);
            int comps = path == 1 ? 4 : 3;
            out.values.resize(val->count);
            for (size_t i = 0; i < val->count; ++i) {
                float v[4] = {0, 0, 0, 0};
                cgltf_accessor_read_float(val, i, v, comps);
                out.values[i] = {v[0], v[1], v[2], v[3]};
            }
            if (!out.times.empty()) clip.duration = std::max(clip.duration, out.times.back());
            clip.channels.push_back(std::move(out));
        }
        out.animations.push_back(std::move(clip));
    }
    cgltf_free(data);
    return out;
}

void AnimationMixer::update(float dt) {
    time_ += dt;
    for (auto& clip : clips_) {
        if (clip.duration <= 0) continue;
        float t = std::fmod(time_, clip.duration);
        for (auto& ch : clip.channels) {
            size_t n = ch.times.size();
            if (!n) continue;
            size_t k = 0;
            while (k + 1 < n && ch.times[k + 1] <= t) ++k;
            auto value = [&](size_t i) { return ch.interp == 2 ? ch.values[i * 3 + 1] : ch.values[i]; };
            glm::vec4 v;
            if (k + 1 >= n || ch.interp == 1 || t <= ch.times[0]) {
                v = value(t <= ch.times[0] ? 0 : k);
            } else {
                float f = (t - ch.times[k]) / std::max(1e-6f, ch.times[k + 1] - ch.times[k]);
                glm::vec4 a = value(k), b = value(k + 1);
                if (ch.path == 1) {
                    glm::quat qa(a.w, a.x, a.y, a.z), qb(b.w, b.x, b.y, b.z);
                    glm::quat q = glm::slerp(qa, qb, f);
                    v = {q.x, q.y, q.z, q.w};
                } else {
                    v = glm::mix(a, b, f);
                }
            }
            if (ch.path == 0) ch.node->position = glm::vec3(v);
            else if (ch.path == 1) ch.node->setQuaternion(glm::normalize(glm::quat(v.w, v.x, v.y, v.z)));
            else ch.node->scale = glm::vec3(v);
        }
    }
}
