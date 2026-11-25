//******** PRACTICA VISUALITZACI� GR�FICA INTERACTIVA (Escola Enginyeria - UAB)
//******** Entorn b�sic VS2026 MONOFINESTRA amb OpenGL 4.6, interf�cie GLFW 3.4, ImGui i llibreries GLM
//******** Enric Mart� (Setembre 2026)
// objLoader.h: Inteface of the class COBJModel.
//
//	  Versi� 2.0:	- Adaptaci� funcions a crear un VAO per a cada material del fitxer
//////////////////////////////////////////////////////////////////////////////////////

#ifndef OBJLOADER_H
#define OBJLOADER_H

#include "platform_compat.h"

#define OBJLOADER_CLASS_DECL     __declspec(dllexport)

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// M�xima mida vector VAOList
#define MAX_SIZE_VAOLIST 125

// Needed structures
struct Vector3D
{
	float fX;
	float fY;
	float fZ;
};

struct Vector2D
{
	float fX;
	float fY;
};

struct OBJFileInfo
{
	unsigned int iVertexCount;
	unsigned int iTexCoordCount;
	unsigned int iNormalCount;
	unsigned int iFaceCount;
	unsigned int iMaterialCount;
};

struct Face
{
	unsigned int	iNumVertices;
	unsigned int	iMaterialIndex;
	Vector3D		*pVertices;
	Vector3D		*pNormals;
	Vector2D		*pTexCoords;
};

struct Material
{
  char	szName[1024];		 // Name of the material
  char	szTexture[_MAX_PATH];// Filename of the texture
  uint	iTextureID;			 // OpenGL name of the texture
  float fDiffuse[3];		 // Diffuse component
  float fAmbient[3];		 // Ambient component
  float fSpecular[3];		 // Specular component
  float fEmmissive[3];		 // Emmissive component
  float fShininess;			 // Specular exponent
};

// Preserve an authored ambient reflectance. For legacy MTL assets with Ka=0,
// provide a restrained diffuse-derived ambient term without editing the asset.
inline bool AplicarFallbackAmbientalDesdeDifuso(Material& material, float factor = 0.18f)
{
	const float mayorKa = (material.fAmbient[0] > material.fAmbient[1]
		? (material.fAmbient[0] > material.fAmbient[2] ? material.fAmbient[0] : material.fAmbient[2])
		: (material.fAmbient[1] > material.fAmbient[2] ? material.fAmbient[1] : material.fAmbient[2]));
	if (mayorKa > 1.0e-4f) return false;

	factor = factor < 0.0f ? 0.0f : (factor > 0.25f ? 0.25f : factor);
	for (int canal = 0; canal < 3; ++canal)
	{
		const float difuso = material.fDiffuse[canal] < 0.0f ? 0.0f :
			(material.fDiffuse[canal] > 1.0f ? 1.0f : material.fDiffuse[canal]);
		material.fAmbient[canal] = difuso * factor;
	}
	return true;
}

class OBJLOADER_CLASS_DECL COBJModel  
{
  public:
	void _stdcall DrawModel(int prim_Id);
	int _stdcall LoadModel(char* szFileName);
	_stdcall COBJModel();
	virtual _stdcall ~COBJModel();
	void _stdcall EliminaLlista(int prim_Id);

// Funcions CVAO
	void _stdcall netejaVAOList_OBJ();
	void _stdcall netejaTextures_OBJ();
	void _stdcall draw_TriVAO_OBJ(GLuint sh_programID);
	void SetInstanceMaterialOverride(GLuint textureId, float shininess) { instanceTextureOverride = textureId; instanceShininessOverride = shininess; }
	bool GetLocalBounds(Vector3D& minimum, Vector3D& maximum) const { if (!hasBounds) return false; minimum = boundsMin; maximum = boundsMax; return true; }

private:
	bool _stdcall ReadNextString(char szString[], FILE *hStream);
//	  int _stdcall LoadTexture(const char szFileName[_MAX_PATH]);
	  int _stdcall LoadTexture2(const char szFileName[_MAX_PATH]);
	  void _stdcall UseMaterial(const Material *pMaterial);
	  void _stdcall UseMaterial_ShaderID(GLuint sh_programID, Material pMaterial);
	  void _stdcall GetTokenParameter(char szString[], const unsigned int iStrSize, FILE *hFile);
	  void _stdcall MakePath(char szFileAndPath[]);
	  bool _stdcall LoadMaterialLib(const char szFileName[], Material *pMaterials,
				unsigned int *iCurMaterialIndex, char szBasePath[]);
	CVAO _stdcall RenderToVAOList(const Face* pFaces, const unsigned int iFaceCount,
				const Material *pMaterials);
	void _stdcall loadToVAOList(const Face* pFaces, const unsigned int iFaceCount,
				const Material* pMaterials);
	void _stdcall GetFaceNormal(float fNormalOut[3], const Face *pFace);
	bool _stdcall ParseFaceString(char szFaceString[], Face *FaceOut, const Vector3D *pVertices,
			  unsigned int vertexCount, const Vector3D *pNormals, unsigned int normalCount,
			  const Vector2D *pTexCoords, unsigned int texCoordCount, const unsigned int iMaterialIndex);
	void _stdcall GetFileInfo(FILE *hStream, OBJFileInfo *Stat, const char szConstBasePath[]);
 	void _stdcall GenTexCoords();

// CVAO
	GLint numMaterials = 0;
	int vector_Materials[MAX_SIZE_VAOLIST]{};
	CVAO VAOList_OBJ[MAX_SIZE_VAOLIST];
	Material vMaterials[MAX_SIZE_VAOLIST]{};
	unsigned int materialCount = 0;
	GLuint instanceTextureOverride = 0;
	float instanceShininessOverride = -1.0f;
	Vector3D boundsMin{}, boundsMax{};
	bool hasBounds = false;

// Funcions CVAO
	void _stdcall initVAOList_OBJ();
	void _stdcall Set_VAOList_OBJ(GLint k, CVAO auxVAO);
	void _stdcall deleteVAOList_OBJ(GLint k);
	void _stdcall draw_TriVAO_Object_OBJ(GLint k);
};

// Callback function for comparing two faces with qsort
static int CompareFaceByMaterial(const void *Arg1, const void *Arg2);

//Returns a newly created COBJModel object. 
OBJLOADER_CLASS_DECL COBJModel* _stdcall InitObject();

//Destroys a COBJModel object
OBJLOADER_CLASS_DECL void _stdcall UnInitObject(COBJModel *obj);

#endif // !defined(AFX_OBJMODEL_H__32C5F722_AD3D_11D1_8F4D_E0B57CC10800__INCLUDED_)
