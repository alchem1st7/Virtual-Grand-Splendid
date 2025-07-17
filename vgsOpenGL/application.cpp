#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <memory>
#include <cstdlib>

#include "tinyfiledialogs.h"
#include "application.h"
#include "imgui.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define GLEW_STATIC
#include "GL/glew.h"

std::string g_thumbnailCacheDir;
std::string g_selectedFolderPath;
std::vector<ImageData> g_images;

GLuint generateTexture(unsigned char* pixels, int width, int height, int channels) {
	if (!pixels || width <= 0 || height <= 0 || channels <= 0) {
		std::cerr << "Invalid pixel data for texture creation." << std::endl;
		return 0;
	}
	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);

	// Set texture wrapping and filtering options
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	GLenum format = GL_RGBA; // Default to RGBA
	if (channels == 3) {
		format = GL_RGB;
	}
	else if (channels == 1) {
		format = GL_RED; 
	}

	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
	glGenerateMipmap(GL_TEXTURE_2D); // Generate mipmaps for better quality at different scales

	glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture
	return textureID;
}

void deleteTexture(GLuint& textureID) {
	if (textureID != 0) {
		glDeleteTextures(1, &textureID);
		textureID = 0;
	}
}

static bool generateThumbnails(const char* inputImagePath, const char* outputImagePath, int newWidth, int newHeight) {
	int width, height, channels;
	unsigned char* imageData = stbi_load(inputImagePath, &width, &height, &channels, STBI_rgb_alpha); // Load as RGBA

	if (!imageData) {
		std::cerr << "Error: Could not load image " << inputImagePath << std::endl;
		return false;
	}

	// Resize to RGBA (4 channels) for consistency, even if original was RGB
	int outputChannels = 4;
	std::vector<unsigned char> resizedImageData(newWidth * newHeight * outputChannels);

	unsigned char* resized_pixels_ptr = stbir_resize_uint8_srgb(
		imageData, width, height, 0,
		resizedImageData.data(), newWidth, newHeight, 0,
		(stbir_pixel_layout)outputChannels
	);

	stbi_image_free(imageData); // Free the original image data

	if (resized_pixels_ptr == NULL) { // Check if resizing failed
		std::cerr << "Error: Failed to resize image for thumbnail." << std::endl;
		return false;
	}

	// Save the resized image as PNG
	if (stbi_write_png(outputImagePath, newWidth, newHeight, outputChannels, resizedImageData.data(), newWidth * outputChannels)) {
		return true;
	}
	else {
		std::cerr << "Error: Could not save resized thumbnail to " << outputImagePath << std::endl;
		return false;
	}
}

void LoadFolder() {
	const char* folder_path = tinyfd_selectFolderDialog(
		"Select a folder",
		NULL   // default path, or NULL
	);


    if (!folder_path) {
        std::cout << "No file selected." << std::endl;
		g_selectedFolderPath.clear();
		// Clear previous images
		for (auto& img : g_images) {
			deleteTexture(img.thumbnailTextureID);
			deleteTexture(img.fullResTextureID);
		}
		g_images.clear();
        return;
    }
	std::cout << "Selected folder: " << folder_path << std::endl;
	g_selectedFolderPath = folder_path;

	// Clear previous images
	for (auto& img : g_images) {
		deleteTexture(img.thumbnailTextureID);
		deleteTexture(img.fullResTextureID);
	}
	g_images.clear();

	// Create a thumbnail cache directory if it doesn't exist
	g_thumbnailCacheDir = g_selectedFolderPath + "/.thumbnails";
	if (!std::filesystem::exists(g_thumbnailCacheDir)) {
		std::filesystem::create_directory(g_thumbnailCacheDir);
	}

	// Supported image extensions
	std::vector<std::string> image_extensions = { ".png", ".jpg", ".jpeg", ".bmp" };

	// Scan the selected folder for images
	for (const auto& entry : std::filesystem::recursive_directory_iterator(g_selectedFolderPath)) {

		if (entry.is_directory() && entry.path() == "/.thumbnails") {
			continue; 
		}

		// Check if the entry is a regular file
		if (entry.is_regular_file()) {
			std::string file_path = entry.path().string();
			std::string file_extension = entry.path().extension().string();
			std::transform(file_extension.begin(), file_extension.end(), file_extension.begin(), ::tolower);
			
			bool isImage = false;
			for (const auto& ext : image_extensions) {
				if (file_extension == ext) {
					isImage = true;
					break;
				}
			}

			if (isImage) {
				ImageData newImage;
				newImage.filePath = file_path;
				newImage.fileName = entry.path().filename().string();
				
				// Generate thumbnail path
				std::string thumbnailFileName = newImage.fileName + ".thumb.png";
				newImage.thumbnailPath = g_thumbnailCacheDir + "/" + thumbnailFileName;

				// Check if thumbnail already exists, otherwise generate it
				if (!std::filesystem::exists(newImage.thumbnailPath)) {
					std::cout << "Generating thumbnail for: " << newImage.fileName << std::endl;
					if (!generateThumbnails(newImage.filePath.c_str(), newImage.thumbnailPath.c_str(), newImage.fullResWidth, newImage.fullResHeight)) {
						std::cerr << "Failed to generate thumbnail for " << newImage.fileName << std::endl;
						continue; 
					}
				}

				// Load thumbnail into OpenGL texture immediately
				int thumbWidth, thumbHeight, thumbChannels;
				unsigned char* thumbPixels = stbi_load(newImage.thumbnailPath.c_str(), &thumbWidth, &thumbHeight, &thumbChannels, STBI_rgb_alpha);
				if (thumbPixels) {
					newImage.thumbnailTextureID = generateTexture(thumbPixels, thumbWidth, thumbHeight, STBI_rgb_alpha);
					newImage.thumbnailWidth = thumbWidth;
					newImage.thumbnailHeight = thumbHeight;
					stbi_image_free(thumbPixels);
				}
				else {
					std::cerr << "Error loading thumbnail for display: " << newImage.thumbnailPath << std::endl;
					// Optionally, use a placeholder texture if thumbnail loading fails
					continue;
				}

				g_images.push_back(std::move(newImage)); 
			}
		}
	}
	std::cout << "Folder loaded successfully. Found " << g_images.size() << " images. Thumbnails are stored in: " << g_thumbnailCacheDir << std::endl;

}

namespace App
{
	static bool showLoadWindow = true;
	static bool showImageWindow = false;

	void RenderUI() {
		// Main controller - this is what gets called from your main loop
		if (showLoadWindow) {
			RenderLoadUI();
		}

		if (showImageWindow) {
			RenderImageGridUI();
		}
	}

	void RenderLoadUI() {
		bool windowOpen = true;

		// Center the window
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
		ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);

		ImGui::Begin("Load folder", &windowOpen,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

		// Center the button in the window
		ImVec2 buttonSize = ImVec2(200, 100);
		ImVec2 avail = ImGui::GetContentRegionAvail();
		float centerX = (avail.x - buttonSize.x) * 0.5f;
		float centerY = (avail.y - buttonSize.y) * 0.5f;

		ImGui::SetCursorPos(ImVec2(centerX, centerY));
		if (ImGui::Button("Load", buttonSize)) {
			LoadFolder();

			// Only switch windows if images were loaded
			if (!g_images.empty()) {
				showLoadWindow = false;
				showImageWindow = true;
			}
		}

		ImGui::End();

		// Handle window close (X button)
		if (!windowOpen) {
			showLoadWindow = false;
			// TODO: You could exit the application here or show a different window
		}
	}

	void RenderImageGridUI() {
		bool windowOpen = true;

		// Center and size the image window
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
		ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

		ImGui::Begin("Image Grid", &windowOpen,
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking);

		ImGui::Separator();

		if (g_images.empty()) {
			ImGui::Text("No images loaded.");
		}
		else {
			// Calculate grid layout
			float windowWidth = ImGui::GetWindowWidth();
			int imagesPerRow = (int)(windowWidth / 160.0f); // 150px image + 10px padding
			if (imagesPerRow < 1) imagesPerRow = 1;

			// Display images in grid
			for (size_t i = 0; i < g_images.size(); ++i) {
				const ImageData& imgData = g_images[i];

				if (imgData.thumbnailTextureID != 0) {
					ImGui::PushID((int)i);

					// Make image clickable
					ImGui::Image((void*)(intptr_t)imgData.thumbnailTextureID,
						ImVec2(150, 150));

					// Tooltip on hover
					if (ImGui::IsItemHovered()) {
						ImGui::BeginTooltip();
						ImGui::Text("%s", imgData.fileName.c_str());
						ImGui::EndTooltip();
					}

					ImGui::PopID();

					// Arrange in grid
					if ((i + 1) % imagesPerRow != 0 && i < g_images.size() - 1) {
						ImGui::SameLine();
					}
				}
			}
		}

		ImGui::End();

		// Handle window close (X button) - go back to load window
		if (!windowOpen) {
			showImageWindow = false;
			showLoadWindow = true;
		}
	}
}