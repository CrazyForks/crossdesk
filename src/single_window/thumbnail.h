/*
 * @Author: DI JUNKUN
 * @Date: 2024-11-07
 * Copyright (c) 2024 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _THUMBNAIL_H_
#define _THUMBNAIL_H_

#include <SDL.h>

#include <filesystem>
#include <map>

class Thumbnail {
 public:
  Thumbnail();
  ~Thumbnail();

 public:
  int SaveToThumbnail(const char* yuv420p, int width, int height,
                      const std::string& host_name,
                      const std::string& remote_id,
                      const std::string& password);

  int LoadThumbnail(SDL_Renderer* renderer,
                    std::map<std::string, SDL_Texture*>& textures, int* width,
                    int* height);

  int DeleteThumbnail(const std::string& file_path);

 private:
  std::vector<std::filesystem::path> FindThumbnailPath(
      const std::filesystem::path& directory);

  std::string AES_encrypt(const std::string& key, const std::string& plaintext);

  std::string AES_decrypt(const std::string& key,
                          const std::string& ciphertext);

 private:
  int thumbnail_width_ = 160;
  int thumbnail_height_ = 90;
  char* rgba_buffer_ = nullptr;
  std::string image_path_ = "thumbnails/";
  std::map<std::time_t, std::filesystem::path> thumbnails_sorted_by_write_time_;

  std::string key_ = "1234567890123456";
  std::string iv_ = "1234567890123456";
};

#endif