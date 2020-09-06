#pragma once

struct QueueFamilyIndices {
	int graphicsFamily = -1;

	bool isValid() const {
		return graphicsFamily >= 0;
	}
};