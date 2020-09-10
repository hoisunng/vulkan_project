#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "Mesh.h"
#include <assimp/scene.h>

class MeshModel {
public:
	MeshModel(std::vector<Mesh> newMeshList);

	size_t getMeshCount() const;
	const Mesh* getMesh(size_t index) const;

	glm::mat4 getModel() const;
	void setModel(glm::mat4 newModel);

	void destroyMeshModel();

	static std::vector<std::string> LoadMaterials(const aiScene *scene);
	static std::vector<Mesh> LoadNode(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, aiNode* node, const aiScene* scene, const std::vector<int>& matToTex);
	static Mesh LoadMesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, aiMesh* mesh, const aiScene* scene, const std::vector<int>& matToTex);

private:
	std::vector<Mesh> meshList;
	glm::mat4 model;
};
