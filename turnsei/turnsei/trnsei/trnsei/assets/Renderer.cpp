#include<glew.h>
#include "Renderer.h"


std::map<char, Text> Texts;

Renderer::Renderer() {
	
}


Renderer::~Renderer() {
}

void Renderer::drawText(std::string text, float x, float y, float scale, glm::vec3 color)
{
	textShader.use();       // シェーダーを有効化
	textShader.setVec3("textColor", color);
	glActiveTexture(GL_TEXTURE0);//テクスチャーを初期化
	glBindVertexArray(textVAO);

	for (std::string::const_iterator c = text.begin(); c != text.end(); c++)
	{
		Text ch = Texts[*c];

		float xpos = x + ch.Bearing.x * scale;
		float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;
		float w = ch.Size.x * scale;
		float h = ch.Size.y * scale;

		
		float vertices[] = { xpos, ypos, w, h };
		glBindBuffer(GL_ARRAY_BUFFER, textVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

	
		glBindTexture(GL_TEXTURE_2D, ch.TextureID);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		
		x += (ch.Advance >> 6) * scale;
	
	}

}