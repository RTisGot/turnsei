#pragma once

#include<glm/glm.hpp>
#include<string>


class Shader {
public:
	unsigned int ID;//シェーダープログラムのID

	int success;//コンパイルの成功を格納する変数
	//vsとfragのパスを受け取るコンストラクタ
	Shader(const char* vertexPath, const char* fragmentPath);
	//(ID(0)メンバ初期化リスト)
	Shader() : ID(0) {}//デフォルトコンストラクタ

	void use();//シェーダーを有効か


	//(&参照として渡す)
	void setVec3(const std::string& name, const glm::vec3& value)const;
	void setMat4(const std::string& name, const glm::mat4& mat)const;
};