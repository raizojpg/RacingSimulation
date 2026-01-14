#version 330 core

in vec4 ex_Color;
in vec2 tex_Coord;

out vec4 out_Color;		

uniform sampler2D myTexture;
uniform int useTexture;

void main(void)
  {
		if(useTexture == 0){
			out_Color = ex_Color;
		}
		else{                              
			out_Color = texture(myTexture, tex_Coord);
		}
  }
 