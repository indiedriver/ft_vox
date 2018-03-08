#include "chunk.hpp"

Chunk::Chunk() : Chunk(glm::ivec3(0)) {}

Chunk::Chunk(glm::ivec3 pos)
    : aabb_center(0.0f), aabb_halfsize(0.0f), _pos(pos), generated(false) {
  _renderAttrib.model = glm::translate(_pos);
  for (int i = 0; i < MODEL_PER_CHUNK; i++) {
    this->dirty[i] = true;
  }
}

Chunk::Chunk(Chunk const& src) { *this = src; }

Chunk::~Chunk(void) {
  for (auto& vao : _renderAttrib.vaos) {
    delete vao;
  }
}

Chunk& Chunk::operator=(Chunk const& rhs) {
  if (this != &rhs) {
    this->_pos = rhs._pos;
    std::memcpy(this->data, rhs.data, sizeof(this->data));
    this->aabb_center = rhs.aabb_center;
    this->aabb_halfsize = rhs.aabb_halfsize;
    this->_renderAttrib.vaos = rhs._renderAttrib.vaos;
    this->_renderAttrib.model = rhs._renderAttrib.model;
    std::memcpy(this->dirty, rhs.dirty, sizeof(this->dirty));
    this->_renderAttrib = rhs._renderAttrib;
    this->_pos = rhs._pos;
    this->generated = rhs.generated;
  }
  return (*this);
}

void Chunk::generate() {
  generated = true;
  generator::generate_chunk(this->data, this->biome_data, glm::vec3(_pos));
  for (int i = 0; i < MODEL_PER_CHUNK; i++) {
    this->dirty[i] = true;
  }
}

void Chunk::mesh(enum MeshingType meshing_type) {
  if (is_dirty()) {
    if (_renderAttrib.vaos.size() != CHUNK_HEIGHT / MODEL_HEIGHT) {
      _renderAttrib.vaos.resize(CHUNK_HEIGHT / MODEL_HEIGHT);
    }
    switch (meshing_type) {
      case MeshingType::Culling:
        mesher::culling(this, _renderAttrib);
        break;
      case MeshingType::Greedy:
        mesher::greedy(this, _renderAttrib);
        break;
      default:
        break;
    }
    mesher::get_aabb(data, aabb_center, aabb_halfsize, _pos);
  }
};

const RenderAttrib& Chunk::getRenderAttrib() { return (this->_renderAttrib); }

inline Block Chunk::get_block(glm::ivec3 index) {
  if (index.x < 0 || index.x >= CHUNK_SIZE || index.y < 0 || index.y >= 256 ||
      index.z < 0 || index.z >= CHUNK_SIZE) {
    Block block = {};
    return (block);
  }
  return (this->data[index.y * CHUNK_SIZE * CHUNK_SIZE + index.x * CHUNK_SIZE +
                     index.z]);
}

inline Biome Chunk::get_biome(glm::ivec3 index) {
  if (index.x < 0 || index.x >= CHUNK_SIZE || index.z < 0 ||
      index.z >= CHUNK_SIZE) {
    Biome biome = {};
    return (biome);
  }
  return (this->biome_data[index.x * CHUNK_SIZE + index.z]);
}

inline void Chunk::set_block(Block block, glm::ivec3 index) {
  this->data[index.y * CHUNK_SIZE * CHUNK_SIZE + index.x * CHUNK_SIZE +
             index.z] = block;
  this->dirty[index.y / MODEL_HEIGHT] = true;
}

glm::ivec3 Chunk::get_pos() { return (_pos); }

bool Chunk::is_dirty() {
  for (int i = 0; i < MODEL_PER_CHUNK; i++) {
    if (dirty[i] == true) {
      return (true);
    }
  }
  return (false);
}

void Chunk::forceFullRemesh() {
  for (int i = 0; i < MODEL_PER_CHUNK; i++) {
    this->dirty[i] = true;
  }
}

void Chunk::setDirty(int model_id) { dirty[model_id] = true; }

ChunkManager::ChunkManager(void) : ChunkManager(42) {}

ChunkManager::ChunkManager(uint32_t seed)
    : _renderDistance(10), _seed(seed), _meshing_type(MeshingType::Greedy) {
  generator::init(10000, _seed);
  if (io::exists("world") == false) {
    io::makedir("world");
  }
  if (io::exists("world/" + std::to_string(_seed)) == false) {
    io::makedir("world/" + std::to_string(_seed));
  }
}

ChunkManager::ChunkManager(ChunkManager const& src) { *this = src; }

ChunkManager::~ChunkManager(void) {
  std::unordered_set<glm::ivec2, ivec2Comparator> regions;
  auto chunk_it = _chunks.begin();
  while (chunk_it != _chunks.end()) {
    glm::ivec2 chunk_pos(chunk_it->first);
    glm::ivec2 region_pos((chunk_pos.x >> 8) * (REGION_SIZE * CHUNK_SIZE),
                          (chunk_pos.y >> 8) * (REGION_SIZE * CHUNK_SIZE));
    regions.insert(region_pos);
    chunk_it++;
  }
  for (const auto& region_pos : regions) {
    unloadRegion(region_pos);
  }
}

ChunkManager& ChunkManager::operator=(ChunkManager const& rhs) {
  if (this != &rhs) {
    this->_renderDistance = rhs._renderDistance;
    this->_meshing_type = rhs._meshing_type;
  }
  return (*this);
}

inline unsigned int getNearestIdx(glm::vec2 position,
                                  const std::deque<glm::ivec2>& positions) {
  unsigned int nearest = 0;
  float min_dist = 99999.0f;
  for (unsigned int i = 0; i < positions.size(); i++) {
    float dist = glm::distance(position, glm::vec2(positions[i]));
    if (dist < min_dist) {
      nearest = i;
      min_dist = dist;
    }
  }
  return (nearest);
}

void ChunkManager::update(const glm::vec3& player_pos) {
  if (to_update.size() > 0) {
    unsigned int nearest_idx =
        getNearestIdx(glm::vec2(player_pos.x, player_pos.z), to_update);
    auto nearest_chunk_it = _chunks.find(to_update[nearest_idx]);
    if (nearest_chunk_it != _chunks.end()) {
      nearest_chunk_it->second.mesh(this->_meshing_type);
      to_mesh.push_back(nearest_chunk_it->first);
    }
    to_update.erase(to_update.begin() + nearest_idx);
  }
  // Find nearest chunk and gen it
  if (to_generate.size() > 0) {
    unsigned int nearest_idx =
        getNearestIdx(glm::vec2(player_pos.x, player_pos.z), to_generate);
    auto nearest_chunk_it = _chunks.find(to_generate[nearest_idx]);
    if (nearest_chunk_it != _chunks.end()) {
      nearest_chunk_it->second.generate();
      to_mesh.push_back(nearest_chunk_it->first);
    }
    to_generate.erase(to_generate.begin() + nearest_idx);
  }
  // Find nearest chunk and mesh it
  if (to_mesh.size() > 0) {
    unsigned int nearest_idx =
        getNearestIdx(glm::vec2(player_pos.x, player_pos.z), to_mesh);
    auto nearest_chunk_it = _chunks.find(to_mesh[nearest_idx]);
    if (nearest_chunk_it != _chunks.end()) {
      nearest_chunk_it->second.mesh(this->_meshing_type);
    }
    to_mesh.erase(to_mesh.begin() + nearest_idx);
  }
  // Add regions within renderDistance
  glm::ivec2 player_chunk_pos =
      glm::ivec2((static_cast<int>(player_pos.x) >> 4) * CHUNK_SIZE,
                 (static_cast<int>(player_pos.z) >> 4) * CHUNK_SIZE);
  for (int x = -this->_renderDistance; x <= this->_renderDistance; x++) {
    for (int z = -this->_renderDistance; z <= this->_renderDistance; z++) {
      glm::ivec2 chunk_pos(player_chunk_pos.x + (x * CHUNK_SIZE),
                           player_chunk_pos.y + (z * CHUNK_SIZE));
      glm::ivec2 region_pos((chunk_pos.x >> 8) * (REGION_SIZE * CHUNK_SIZE),
                            (chunk_pos.y >> 8) * (REGION_SIZE * CHUNK_SIZE));
      addRegionToQueue(region_pos);
    }
  }
  // Unload regions
  unloadRegions(player_chunk_pos);
  if (to_unload.size() > 0) {
    unloadRegion(to_unload.front());
    to_unload.pop_front();
  }
}

void ChunkManager::addRegionToQueue(glm::ivec2 region_pos) {
  auto chunk = _chunks.find(region_pos);
  if (chunk == _chunks.end()) {
    // TODO : Defer this (1 max per frame)
    loadRegion(region_pos);
  }
}

std::string ChunkManager::getRegionFilename(glm::ivec2 pos) {
  std::string filename = "world/" + std::to_string(_seed) + "/r." +
                         std::to_string(pos.x / REGION_SIZE) + "." +
                         std::to_string(pos.y / REGION_SIZE) + ".vox";
  return (filename);
}

void ChunkManager::loadRegion(glm::ivec2 region_pos) {
  unsigned char chunk_rle[(CHUNK_SIZE * CHUNK_SIZE * CHUNK_HEIGHT) * 2] = {0};
  unsigned char lookup[REGION_LOOKUPTABLE_SIZE] = {0};

  std::string filename = getRegionFilename(region_pos);
  if (io::exists(filename) == false) {
    io::initRegionFile(filename);
  }
  unsigned int filesize = io::get_filesize(filename);
  FILE* region = fopen(filename.c_str(), "rb+");
  if (region != NULL) {
    if (filesize < REGION_LOOKUPTABLE_SIZE) {
      fclose(region);
      return;
    }
    fread(lookup, REGION_LOOKUPTABLE_SIZE, 1, region);

    int file_offset = REGION_LOOKUPTABLE_SIZE;
    for (int y = 0; y < REGION_SIZE; y++) {
      for (int x = 0; x < REGION_SIZE; x++) {
        int lookup_offset = 3 * (x + y * REGION_SIZE);
        unsigned int content_size =
            ((unsigned int)(lookup[lookup_offset + 0]) << 16) |
            ((unsigned int)(lookup[lookup_offset + 1]) << 8) |
            ((unsigned int)(lookup[lookup_offset + 2]));

        glm::ivec2 chunk_position = glm::ivec2(region_pos.x + (x * CHUNK_SIZE),
                                               region_pos.y + (y * CHUNK_SIZE));
        auto emplace_res = _chunks.emplace(
            chunk_position, Chunk({chunk_position.x, 0, chunk_position.y}));
        auto chunk_it = emplace_res.first;
        if (content_size != 0) {
          // Chunk already generated and saved on disk, just decode and mesh
          // it back
          if (filesize < file_offset + content_size) {
            fclose(region);
            return;
          }
          fseek(region, file_offset, SEEK_SET);
          fread(chunk_rle, content_size, 1, region);
          io::decodeRLE(chunk_rle, content_size, chunk_it->second.data,
                        (CHUNK_SIZE * CHUNK_SIZE * CHUNK_HEIGHT));
          chunk_it->second.generated = true;
          this->to_mesh.push_back(chunk_it->first);
        } else {
          this->to_generate.push_back(chunk_it->first);
        }
        file_offset += content_size;
      }
    }
    fclose(region);
  }
}

void ChunkManager::eraseUnloadedChunk(glm::ivec2 pos) {
  // Remove chunk from queues
  for (auto it = to_mesh.begin(); it != to_mesh.end(); it++) {
    if (*it == pos) {
      to_mesh.erase(it);
      break;
    }
  }
  for (auto it = to_generate.begin(); it != to_generate.end(); it++) {
    if (*it == pos) {
      to_generate.erase(it);
      break;
    }
  }
}

void ChunkManager::unloadRegion(glm::ivec2 region_pos) {
  unsigned char chunk_rle[(CHUNK_SIZE * CHUNK_SIZE * CHUNK_HEIGHT) * 2] = {0};
  unsigned char lookup[REGION_LOOKUPTABLE_SIZE] = {0};

  std::string filename = getRegionFilename(region_pos);
  if (io::exists(filename) == false) {
    io::initRegionFile(filename);
  }
  FILE* region = fopen(filename.c_str(), "rb+");
  if (region != NULL) {
    fread(lookup, REGION_LOOKUPTABLE_SIZE, 1, region);

    int file_offset = REGION_LOOKUPTABLE_SIZE;
    for (int y = 0; y < REGION_SIZE; y++) {
      for (int x = 0; x < REGION_SIZE; x++) {
        int lookup_offset = 3 * (x + y * REGION_SIZE);
        unsigned int content_size = 0;

        glm::ivec2 chunk_position = glm::ivec2(region_pos.x + (x * CHUNK_SIZE),
                                               region_pos.y + (y * CHUNK_SIZE));
        auto chunk_it = _chunks.find(chunk_position);
        if (chunk_it != _chunks.end()) {
          if (chunk_it->second.generated) {
            std::memset(chunk_rle, 0,
                        (CHUNK_SIZE * CHUNK_SIZE * CHUNK_HEIGHT) * 2);
            unsigned int len_rle = static_cast<unsigned int>(
                io::encodeRLE(chunk_it->second.data, chunk_rle));
            lookup[lookup_offset + 0] = (len_rle & 0xff0000) >> 16;
            lookup[lookup_offset + 1] = (len_rle & 0xff00) >> 8;
            lookup[lookup_offset + 2] = (len_rle & 0xff);
            content_size = len_rle;
            fseek(region, file_offset, SEEK_SET);
            fwrite(chunk_rle, len_rle, 1, region);
          }
          eraseUnloadedChunk(chunk_it->first);
          _chunks.erase(chunk_it);
        }
        file_offset += content_size;
      }
    }
    fseek(region, 0, SEEK_SET);
    fwrite(lookup, REGION_LOOKUPTABLE_SIZE, 1, region);
    fclose(region);
  }
}

void ChunkManager::unloadRegions(glm::ivec2 current_chunk_pos) {
  std::unordered_set<glm::ivec2, ivec2Comparator> regions;
  auto chunk_it = _chunks.begin();
  while (chunk_it != _chunks.end()) {
    glm::ivec2 chunk_pos(chunk_it->first);
    glm::ivec2 region_pos((chunk_pos.x >> 8) * (REGION_SIZE * CHUNK_SIZE),
                          (chunk_pos.y >> 8) * (REGION_SIZE * CHUNK_SIZE));
    regions.insert(region_pos);
    chunk_it++;
  }
  if (regions.size() > 0) {
    for (auto region : regions) {
      glm::ivec2 region_center = region + ((REGION_SIZE * CHUNK_SIZE) / 2);
      float dist = fabs(glm::distance(glm::vec2(current_chunk_pos),
                                      glm::vec2(region_center)));
      if (round(dist) / CHUNK_SIZE >
          static_cast<float>(this->_renderDistance + (REGION_SIZE + 1))) {
        to_unload.push_back(region);
      }
    }
  }
}

void ChunkManager::setRenderAttributes(Renderer& renderer,
                                       glm::vec3 player_pos) {
  glm::ivec2 pos =
      glm::ivec2((static_cast<int>(round(player_pos.x)) -
                  (static_cast<int>(round(player_pos.x)) % CHUNK_SIZE)),
                 (static_cast<int>(round(player_pos.z)) -
                  (static_cast<int>(round(player_pos.z)) % CHUNK_SIZE)));
  frustrum_culling.updateViewPlanes(renderer.uniforms.view_proj);

  _debug_chunks_rendered = 0;
  auto chunk_it = _chunks.begin();
  while (chunk_it != _chunks.end()) {
    glm::ivec3 c_pos = chunk_it->second.get_pos();
    float dist = glm::distance(glm::vec2(c_pos.x, c_pos.z), glm::vec2(pos));
    if (round(dist) / CHUNK_SIZE <
        static_cast<float>(this->_renderDistance + 1)) {
      if (frustrum_culling.cull(chunk_it->second.aabb_center,
                                chunk_it->second.aabb_halfsize)) {
        renderer.addRenderAttrib(chunk_it->second.getRenderAttrib());
        _debug_chunks_rendered++;
      }
    }
    chunk_it++;
  }
}

inline Block ChunkManager::get_block(glm::ivec3 index) {
  glm::ivec2 chunk_pos =
      glm::ivec2((index.x >> 4) * CHUNK_SIZE, (index.z >> 4) * CHUNK_SIZE);
  auto chunk_it = _chunks.find(chunk_pos);
  if (chunk_it != _chunks.end()) {
    glm::ivec3 block_pos;
    block_pos.x = index.x - chunk_pos.x;
    block_pos.y = index.y;
    block_pos.z = index.z - chunk_pos.y;
    return (chunk_it->second.get_block(block_pos));
  }
  Block block = {};
  return (block);
}

inline void ChunkManager::set_block(Block block, glm::ivec3 index) {
  glm::ivec2 chunk_pos =
      glm::ivec2((index.x >> 4) * CHUNK_SIZE, (index.z >> 4) * CHUNK_SIZE);
  auto chunk_it = _chunks.find(chunk_pos);
  if (chunk_it != _chunks.end()) {
    glm::ivec3 block_pos;
    block_pos.x = index.x - chunk_pos.x;
    block_pos.y = index.y;
    block_pos.z = index.z - chunk_pos.y;
    if (_meshing_type == MeshingType::Greedy) {
      // Block extents doesn't necessary match model boundary
      // Query his real size to update every model impacted
      /*glm::ivec3 size = mesher::get_interval(chunk_it->second.data, block_pos,
                                             get_block(block_pos));
      for (int i = block_pos.y - (size.y + 1); i < block_pos.y + (size.y + 1);
           i++) {
        chunk_it->second.setDirty(block_pos.y / MODEL_HEIGHT);
      }*/
      chunk_it->second.forceFullRemesh();
    }
    chunk_it->second.set_block(block, block_pos);
    to_update.push_back(chunk_it->first);
  }
}

inline float intbound(float pos, float ds) {
  return (ds > 0.0f ? ceil(pos) - pos : pos - floor(pos)) / fabs(ds);
}

void ChunkManager::rayCast(glm::vec3 ray_dir, glm::vec3 ray_pos,
                           float max_dist) {
  // http://www.cse.chalmers.se/edu/year/2011/course/TDA361/grid.pdf
  glm::ivec3 pos = glm::floor(ray_pos);
  glm::ivec3 step;
  step.x = ray_dir.x < 0.0f ? -1 : 1;
  step.y = ray_dir.y < 0.0f ? -1 : 1;
  step.z = ray_dir.z < 0.0f ? -1 : 1;
  glm::vec3 tMax;
  glm::vec3 delta = glm::vec3(step) / ray_dir;
  tMax.x = intbound(ray_pos.x, ray_dir.x);
  tMax.y = intbound(ray_pos.y, ray_dir.y);
  tMax.z = intbound(ray_pos.z, ray_dir.z);
  Block block = {};
  while (1) {
    if (pos.y > 255 || pos.y < 0) {
      break;
    }
    if (block.material != Material::Air) {
      break;
    }
    if (tMax.x < tMax.y) {
      if (tMax.x < tMax.z) {
        if (tMax.x > max_dist) break;
        pos.x += step.x;
        tMax.x += delta.x;
      } else {
        if (tMax.z > max_dist) break;
        pos.z += step.z;
        tMax.z += delta.z;
      }
    } else {
      if (tMax.y < tMax.z) {
        if (tMax.y > max_dist) break;
        pos.y += step.y;
        tMax.y += delta.y;
      } else {
        if (tMax.z > max_dist) break;
        pos.z += step.z;
        tMax.z += delta.z;
      }
    }
    block = get_block(pos);
  }
  if (block.material != Material::Air) {
    Block air = {};
    set_block(air, pos);
  }
}

void ChunkManager::setRenderDistance(unsigned char rd) {
  this->_renderDistance = rd;
}

void ChunkManager::increaseRenderDistance() {
  if (this->_renderDistance + 1 <= 20) this->_renderDistance++;
}

void ChunkManager::decreaseRenderDistance() {
  if (this->_renderDistance - 1 > 0) this->_renderDistance--;
}

void ChunkManager::setMeshingType(enum MeshingType type) {
  _meshing_type = type;
  reloadMesh();
}

void ChunkManager::reloadMesh() {
  to_mesh.clear();
  auto chunk_it = _chunks.begin();
  while (chunk_it != _chunks.end()) {
    chunk_it->second.forceFullRemesh();
    to_mesh.push_back(chunk_it->first);
    chunk_it++;
  }
}

void ChunkManager::print_chunkmanager_info(Renderer& renderer, float fheight,
                                           float fwidth) {
  renderer.renderText(
      10.0f, fheight - 75.0f, 0.35f,
      "chunks: " + std::to_string(_chunks.size()) +
          ", rendered: " + std::to_string(_debug_chunks_rendered),
      glm::vec3(1.0f, 1.0f, 1.0f));
  renderer.renderText(10.0f, fheight - 100.0f, 0.35f,
                      "queue: mesh(" + std::to_string(to_mesh.size()) +
                          ") generate(" + std::to_string(to_generate.size()) +
                          ") unload(" + std::to_string(to_unload.size()) + ")",
                      glm::vec3(1.0f, 1.0f, 1.0f));
  renderer.renderText(10.0f, fheight - 125.0f, 0.35f,
                      "render distance: " + std::to_string(_renderDistance),
                      glm::vec3(1.0f, 1.0f, 1.0f));
}
