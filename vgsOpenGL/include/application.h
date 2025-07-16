#pragma once

#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <memory>
#include <cstdlib>

// Forward declarations for OpenGL types to avoid including GL/glew.h here
// This is good practice for headers to reduce compilation dependencies.
typedef unsigned int GLuint;

// Structure to hold image information
// This needs to be declared in the header so other files can use it.
// Estructura para almacenar información de la imagen
struct ImageData {
    std::string filePath;
    std::string thumbnailPath;
    std::string fileName;

    GLuint thumbnailTextureID = 0;
    int thumbnailWidth = 0;
    int thumbnailHeight = 0;

    GLuint fullResTextureID = 0;
    int fullResWidth = 0;
    int fullResHeight = 0;

    std::atomic<bool> isLoadingFullRes = false;
    std::atomic<bool> fullResLoaded = false;

    // --- Add a custom move constructor ---
    ImageData(ImageData&& other) noexcept
        : filePath(std::move(other.filePath)),
        thumbnailPath(std::move(other.thumbnailPath)),
        fileName(std::move(other.fileName)),
        thumbnailTextureID(other.thumbnailTextureID),
        thumbnailWidth(other.thumbnailWidth),
        thumbnailHeight(other.thumbnailHeight),
        fullResTextureID(other.fullResTextureID),
        fullResWidth(other.fullResWidth),
        fullResHeight(other.fullResHeight),
        isLoadingFullRes(other.isLoadingFullRes.load()), // Atomically load value
        fullResLoaded(other.fullResLoaded.load())         // Atomically load value
    {
        // Reset other's texture IDs to prevent double deletion
        other.thumbnailTextureID = 0;
        other.fullResTextureID = 0;
    }

    // --- Add a custom move assignment operator ---
    ImageData& operator=(ImageData&& other) noexcept {
        if (this != &other) {
            // Clean up current resources before moving
            // If you had textures loaded here, you'd delete them
            // deleteTexture(thumbnailTextureID); // Assuming deleteTexture is available
            // deleteTexture(fullResTextureID);

            filePath = std::move(other.filePath);
            thumbnailPath = std::move(other.thumbnailPath);
            fileName = std::move(other.fileName);
            thumbnailTextureID = other.thumbnailTextureID;
            thumbnailWidth = other.thumbnailWidth;
            thumbnailHeight = other.thumbnailHeight;
            fullResTextureID = other.fullResTextureID;
            fullResWidth = other.fullResWidth;
            fullResHeight = other.fullResHeight;
            isLoadingFullRes = other.isLoadingFullRes.load();
            fullResLoaded = other.fullResLoaded.load();

            // Reset other's texture IDs to prevent double deletion
            other.thumbnailTextureID = 0;
            other.fullResTextureID = 0;
        }
        return *this;
    }

    // --- Delete copy constructor and copy assignment operator ---
    // This explicitly tells the compiler that ImageData cannot be copied.
    ImageData(const ImageData&) = delete;
    ImageData& operator=(const ImageData&) = delete;

    // --- Default constructor is still needed ---
    ImageData() = default;

    // --- Destructor to clean up OpenGL textures ---
    // This is important when ImageData objects are destroyed (e.g., when g_images is cleared)
    // You'll need to define deleteTexture in your .cpp or ensure it's linked.
    ~ImageData() {
        // Assuming deleteTexture is defined in your .cpp or globally accessible
        // deleteTexture(thumbnailTextureID);
        // deleteTexture(fullResTextureID);
    }
};

extern std::string g_thumbnailCacheDir;
extern std::string g_selectedFolderPath;
extern std::vector<ImageData> g_images; 

void LoadFolder(); 

namespace App
{
    void RenderUI();
    void RenderLoadUI();
	void RenderImageGridUI();
}