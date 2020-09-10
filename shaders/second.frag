#version 450 // Use GLSL 4.5

layout (input_attachment_index = 0, binding = 0) uniform subpassInput inputColour;
layout (input_attachment_index = 1, binding = 1) uniform subpassInput inputDepth;

layout (location = 0) out vec4 colour;

void main() {
	int xHalf = 800 / 2;
	if (gl_FragCoord.x > xHalf) {
		float lowerBound = 0.98;
		float upperBound = 1;

		float depth = subpassLoad(inputDepth).r;
		float depthColourScaled = 1.0f - ((depth - lowerBound) / (upperBound - lowerBound));
		colour = vec4(subpassLoad(inputColour).rgb * depthColourScaled, 1.0f);
	} else {
		colour = subpassLoad(inputColour).rgba;
	}
}
