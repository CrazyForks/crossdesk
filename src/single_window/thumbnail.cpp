#include "thumbnail.h"

#include <openssl/aes.h>
#include <openssl/rand.h>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "libyuv.h"
#include "rd_log.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static std::string test;

void ScaleYUV420pToABGR(char* dst_buffer_, int video_width_, int video_height_,
                        int scaled_video_width_, int scaled_video_height_,
                        char* rgba_buffer_) {
  int src_y_size = video_width_ * video_height_;
  int src_uv_size = (video_width_ + 1) / 2 * (video_height_ + 1) / 2;
  int dst_y_size = scaled_video_width_ * scaled_video_height_;
  int dst_uv_size =
      (scaled_video_width_ + 1) / 2 * (scaled_video_height_ + 1) / 2;

  uint8_t* src_y = reinterpret_cast<uint8_t*>(dst_buffer_);
  uint8_t* src_u = src_y + src_y_size;
  uint8_t* src_v = src_u + src_uv_size;

  std::unique_ptr<uint8_t[]> dst_y(new uint8_t[dst_y_size]);
  std::unique_ptr<uint8_t[]> dst_u(new uint8_t[dst_uv_size]);
  std::unique_ptr<uint8_t[]> dst_v(new uint8_t[dst_uv_size]);

  try {
    libyuv::I420Scale(src_y, video_width_, src_u, (video_width_ + 1) / 2, src_v,
                      (video_width_ + 1) / 2, video_width_, video_height_,
                      dst_y.get(), scaled_video_width_, dst_u.get(),
                      (scaled_video_width_ + 1) / 2, dst_v.get(),
                      (scaled_video_width_ + 1) / 2, scaled_video_width_,
                      scaled_video_height_, libyuv::kFilterBilinear);
  } catch (const std::exception& e) {
    LOG_ERROR("I420Scale failed: %s", e.what());
    return;
  }

  try {
    libyuv::I420ToABGR(
        dst_y.get(), scaled_video_width_, dst_u.get(),
        (scaled_video_width_ + 1) / 2, dst_v.get(),
        (scaled_video_width_ + 1) / 2, reinterpret_cast<uint8_t*>(rgba_buffer_),
        scaled_video_width_ * 4, scaled_video_width_, scaled_video_height_);
  } catch (const std::exception& e) {
    LOG_ERROR("I420ToRGBA failed: %s", e.what());
    return;
  }
}

Thumbnail::Thumbnail() { std::filesystem::create_directory(image_path_); }

Thumbnail::~Thumbnail() {
  if (rgba_buffer_) {
    delete[] rgba_buffer_;
    rgba_buffer_ = nullptr;
  }
}

int Thumbnail::SaveToThumbnail(const char* yuv420p, int width, int height,
                               const std::string& host_name,
                               const std::string& remote_id,
                               const std::string& password) {
  if (!rgba_buffer_) {
    rgba_buffer_ = new char[thumbnail_width_ * thumbnail_height_ * 4];
  }

  if (yuv420p) {
    ScaleYUV420pToABGR((char*)yuv420p, width, height, thumbnail_width_,
                       thumbnail_height_, rgba_buffer_);

    std::string image_name = password + "@" + host_name + "@" + remote_id;
    LOG_ERROR("1 Save thumbnail: {}", image_name);
    std::string cipher_image_name = AES_encrypt(key_, image_name);
    LOG_ERROR("2 Save thumbnail: {}", cipher_image_name);
    std::string save_path = image_path_ + cipher_image_name;
    stbi_write_png(save_path.data(), thumbnail_width_, thumbnail_height_, 4,
                   rgba_buffer_, thumbnail_width_ * 4);
  }
  return 0;
}

bool LoadTextureFromMemory(const void* data, size_t data_size,
                           SDL_Renderer* renderer, SDL_Texture** out_texture,
                           int* out_width, int* out_height) {
  int image_width = 0;
  int image_height = 0;
  int channels = 4;
  unsigned char* image_data =
      stbi_load_from_memory((const unsigned char*)data, (int)data_size,
                            &image_width, &image_height, NULL, 4);
  if (image_data == nullptr) {
    fprintf(stderr, "Failed to load image: %s\n", stbi_failure_reason());
    return false;
  }

  // ABGR
  SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
      (void*)image_data, image_width, image_height, channels * 8,
      channels * image_width, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
  if (surface == nullptr) {
    fprintf(stderr, "Failed to create SDL surface: %s\n", SDL_GetError());
    return false;
  }

  SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (texture == nullptr)
    fprintf(stderr, "Failed to create SDL texture: %s\n", SDL_GetError());

  *out_texture = texture;
  *out_width = image_width;
  *out_height = image_height;

  SDL_FreeSurface(surface);
  stbi_image_free(image_data);

  return true;
}

bool LoadTextureFromFile(const char* file_name, SDL_Renderer* renderer,
                         SDL_Texture** out_texture, int* out_width,
                         int* out_height) {
  std::filesystem::path file_path(file_name);
  if (!std::filesystem::exists(file_path)) return false;
  std::ifstream file(file_path, std::ios::binary);
  if (!file) return false;
  file.seekg(0, std::ios::end);
  size_t file_size = file.tellg();
  file.seekg(0, std::ios::beg);
  if (file_size == -1) return false;
  char* file_data = new char[file_size];
  if (!file_data) return false;
  file.read(file_data, file_size);
  bool ret = LoadTextureFromMemory(file_data, file_size, renderer, out_texture,
                                   out_width, out_height);
  delete[] file_data;

  return ret;
}

std::vector<std::filesystem::path> Thumbnail::FindThumbnailPath(
    const std::filesystem::path& directory) {
  std::vector<std::filesystem::path> thumbnails_path;
  // std::string image_extensions = ".png";

  if (!std::filesystem::is_directory(directory)) {
    LOG_ERROR("No such directory [{}]", directory.string());
    return thumbnails_path;
  }

  thumbnails_sorted_by_write_time_.clear();

  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    if (entry.is_regular_file()) {
      std::time_t last_write_time = std::chrono::system_clock::to_time_t(
          time_point_cast<std::chrono::system_clock::duration>(
              entry.last_write_time() -
              std::filesystem::file_time_type::clock::now() +
              std::chrono::system_clock::now()));

      // if (entry.path().extension() == image_extensions) {
      thumbnails_sorted_by_write_time_[last_write_time] = entry.path();
      // }
    }
  }

  for (auto it = thumbnails_sorted_by_write_time_.rbegin();
       it != thumbnails_sorted_by_write_time_.rend(); ++it) {
    thumbnails_path.push_back(it->second);
  }

  return thumbnails_path;
}

int Thumbnail::LoadThumbnail(SDL_Renderer* renderer,
                             std::map<std::string, SDL_Texture*>& textures,
                             int* width, int* height) {
  textures.clear();
  std::vector<std::filesystem::path> image_paths =
      FindThumbnailPath(image_path_);

  if (image_paths.size() == 0) {
    return -1;
  } else {
    for (int i = 0; i < image_paths.size(); i++) {
      size_t pos1 = image_paths[i].string().find('/') + 1;
      std::string cipher_image_name = image_paths[i].string().substr(pos1);
      LOG_ERROR("cipher_image_name: {}", cipher_image_name);
      std::string original_image_name = AES_decrypt(key_, cipher_image_name);
      std::string image_path = image_path_ + original_image_name;
      LOG_ERROR("image_path: {}", image_path);
      // size_t pos1 = original_image_name[i].string().find('@') + 1;
      // size_t pos2 = original_image_name[i].string().rfind('@');
      // std::string password = original_image_name[i].string().substr(0, pos1);
      // std::string host_name =
      //     original_image_name[i].string().substr(pos1, pos2 - pos1);
      // std::string remote_id = original_image_name[i].string().substr(pos2 +
      // 1);

      textures[original_image_name] = nullptr;
      LoadTextureFromFile(image_path.c_str(), renderer,
                          &(textures[original_image_name]), width, height);
    }
    return 0;
  }
  return 0;
}

int Thumbnail::DeleteThumbnail(const std::string& file_path) {
  if (std::filesystem::exists(file_path)) {
    std::filesystem::remove(file_path);
    LOG_INFO("File [{}] removed successfully", file_path);
    return 0;
  } else {
    LOG_ERROR("File [{}] does not exist", file_path);
    return -1;
  }
}

// 将std::string转换为unsigned char向量
std::vector<unsigned char> string_to_uchar_vector(const std::string& str) {
  return std::vector<unsigned char>(str.begin(), str.end());
}

// 将unsigned char向量转换为std::string
std::string uchar_vector_to_string(const std::vector<unsigned char>& vec) {
  return std::string(vec.begin(), vec.end());
}

// PKCS#7 填充
void pkcs7_pad(std::vector<unsigned char>& data) {
  size_t pad_length = AES_BLOCK_SIZE - (data.size() % AES_BLOCK_SIZE);
  data.insert(data.end(), pad_length, static_cast<unsigned char>(pad_length));
}

// PKCS#7 去除填充
void pkcs7_unpad(std::vector<unsigned char>& data) {
  if (!data.empty()) {
    size_t pad_length = data.back();
    data.resize(data.size() - pad_length);
  }
}

std::string Thumbnail::AES_encrypt(const std::string& key,
                                   const std::string& plaintext) {
  std::vector<unsigned char> key_vec = string_to_uchar_vector(key);
  std::vector<unsigned char> iv(AES_BLOCK_SIZE);
  RAND_bytes(iv.data(), AES_BLOCK_SIZE);  // 随机生成IV

  std::vector<unsigned char> plaintext_vec = string_to_uchar_vector(plaintext);
  pkcs7_pad(plaintext_vec);  // 填充明文

  std::vector<unsigned char> ciphertext(plaintext_vec.size());
  AES_KEY encryptKey;
  AES_set_encrypt_key(key_vec.data(), 128, &encryptKey);

  AES_cbc_encrypt(plaintext_vec.data(), ciphertext.data(), plaintext_vec.size(),
                  &encryptKey, iv.data(), AES_ENCRYPT);

  // 将IV和密文拼接，方便解密时取出IV
  ciphertext.insert(ciphertext.begin(), iv.begin(), iv.end());

  // return uchar_vector_to_string(ciphertext);

  std::string encrypted = uchar_vector_to_string(ciphertext);

  std::string original_image_name =
      AES_decrypt(key_, uchar_vector_to_string(ciphertext));
  LOG_ERROR("!!!!!!!!!!!!!!! src = [{}]", original_image_name);

  // 转换成十六进制字符串
  std::ostringstream encrypted_oss;
  for (unsigned char c : encrypted) {
    encrypted_oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
  }
  return encrypted_oss.str();
}

std::string Thumbnail::AES_decrypt(const std::string& key,
                                   const std::string& ciphertext) {
  // // 将十六进制字符串转换回原始二进制密文
  std::string original_ciphertext = ciphertext;
  // for (size_t i = 0; i < ciphertext.size(); i += 2) {
  //   std::string byte_str = ciphertext.substr(i, 2);
  //   unsigned char byte =
  //       static_cast<unsigned char>(std::stoi(byte_str, nullptr, 16));
  //   original_ciphertext.push_back(byte);
  // }

  std::vector<unsigned char> key_vec = string_to_uchar_vector(key);
  std::vector<unsigned char> ciphertext_vec =
      string_to_uchar_vector(ciphertext);

  // 提取IV
  std::vector<unsigned char> iv(ciphertext_vec.begin(),
                                ciphertext_vec.begin() + AES_BLOCK_SIZE);
  ciphertext_vec.erase(ciphertext_vec.begin(),
                       ciphertext_vec.begin() + AES_BLOCK_SIZE);

  std::vector<unsigned char> plaintext(ciphertext_vec.size());
  AES_KEY decryptKey;
  AES_set_decrypt_key(key_vec.data(), 128, &decryptKey);

  AES_cbc_encrypt(ciphertext_vec.data(), plaintext.data(),
                  ciphertext_vec.size(), &decryptKey, iv.data(), AES_DECRYPT);

  pkcs7_unpad(plaintext);  // 去除填充

  return uchar_vector_to_string(plaintext);
}
