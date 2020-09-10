#include "MeshModel.h"

MeshModel::MeshModel(std::vector<Mesh> newMeshList)
{
	meshList = newMeshList;
	model = glm::mat4{ 1.0f };
}

size_t MeshModel::getMeshCount() const
{
	return meshList.size();
}

const Mesh* MeshModel::getMesh(size_t index) const
{
	return &meshList.at(index);
}

glm::mat4 MeshModel::getModel() const
{
	return model;
}

void MeshModel::setModel(glm::mat4 newModel)
{
	model = newModel;
}

void MeshModel::destroyMeshModel()
{
	for (auto& mesh : meshList) {
		mesh.destroyBuffers();
	}
}

std::vector<std::string> MeshModel::LoadMaterials(const aiScene* scene)
{
	std::vector<std::string> textureList{ scene->mNumMaterials };

	for (auto i = 0u; i < scene->mNumMaterials; ++i) {
		const auto material = scene->mMaterials[i];

		textureList[i] = "";
		if (material->GetTextureCount(aiTextureType_DIFFUSE)) {
			aiString path;
			if (material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
				const auto idx = std::string{ path.data }.rfind('\\');
				const auto fileName = std::string{ path.data }.substr(idx + 1);
				textureList[i] = fileName;
			}
		}
	}

	return textureList;
}

std::vector<Mesh> MeshModel::LoadNode(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, aiNode* node, const aiScene* scene, const std::vector<int>& matToTex)
{
	std::vector<Mesh> meshList;

	for (auto i = 0u; i < node->mNumMeshes; ++i) {
		meshList.push_back(LoadMesh(newPhysicalDevice, newDevice, transferQueue, transferCommandPool, scene->mMeshes[node->mMeshes[i]], scene, matToTex));
	}

	for (auto i = 0u; i < node->mNumChildren; ++i) {
		const auto newList = LoadNode(newPhysicalDevice, newDevice, transferQueue, transferCommandPool, node->mChildren[i], scene, matToTex);
		meshList.insert(meshList.end(), newList.begin(), newList.end());
	}

	return meshList;
}

Mesh MeshModel::LoadMesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, aiMesh* mesh, const aiScene* scene, const std::vector<int>& matToTex)
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	vertices.resize(mesh->mNumVertices);
	for (auto i = 0u; i < mesh->mNumVertices; ++i) {
		vertices[i].pos = { mesh->mVertices[i].x,mesh->mVertices[i].y,mesh->mVertices[i].z };

		if (mesh->mTextureCoords[0]) {
			vertices[i].tex = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
		}
		else {
			vertices[i].tex = { 0.0f, 0.0f };
		}
		
		vertices[i].col = { 1.0f, 1.0f, 1.0f };
	}

	for (auto i = 0u; i < mesh->mNumFaces; ++i) {
		const auto face = mesh->mFaces[i];
		for (auto j = 0u; j < face.mNumIndices; ++j) {
			indices.push_back(face.mIndices[j]);
		}
	}

	return Mesh{newPhysicalDevice, newDevice, transferQueue, transferCommandPool, &vertices, &indices, matToTex[mesh->mMaterialIndex]};
}
