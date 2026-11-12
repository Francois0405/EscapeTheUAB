//******** PRACTICA VISUALITZACI� GR�FICA INTERACTIVA (Escola Enginyeria - UAB)
//******** Entorn b�sic VS2026 MONOFINESTRA amb OpenGL 4.6, interf�cie GLFW 3.4, ImGui i llibreries GLM
//******** Enric Mart� (Setembre 2026)
// objLoader.cpp: Implements the class COBJModel.
//
//	  Versi� 2.0:	- Adaptaci� funcions a crear un VAO per a cada material del fitxer
//////////////////////////////////////////////////////////////////////////////////////
//           Wavefront OBJ Loader (C) 2000 Tim C. Schr�der
// -------------------------------------------------------------------
//    tcs_web@gmx.de / tcs_web@hotmail.com / tcs@thereisnofate.net
//                 http://glvelocity.demonews.com
//////////////////////////////////////////////////////////////////////

#include "framework.h"
#include "objLoader.h"
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Resolucion de rutas portable (macOS/Linux distinguen mayusculas, usan '/'
// y los MTL exportados en Windows contienen '\\' o rutas absolutas "C:/...").
// ---------------------------------------------------------------------------
namespace {
namespace fs = std::filesystem;

std::string LowerStr(std::string v)
{
	for (char& c : v) c = (char)std::tolower((unsigned char)c);
	return v;
}

// Busca "name" dentro de "dir" ignorando mayusculas/minusculas.
bool FindNoCase(const fs::path& dir, const std::string& name, fs::path& out)
{
	std::error_code ec;
	fs::path direct = dir / name;
	if (fs::exists(direct, ec)) { out = direct; return true; }
	const std::string lname = LowerStr(name);
	for (fs::directory_iterator it(dir.empty() ? fs::path(".") : dir, ec), end; !ec && it != end; it.increment(ec))
		if (LowerStr(it->path().filename().string()) == lname) { out = it->path(); return true; }
	return false;
}

// Resuelve una ruta relativa componente a componente sin distinguir mayusculas.
bool ResolveNoCase(const fs::path& base, const std::string& rel, fs::path& out)
{
	fs::path cur = base;
	std::stringstream ss(rel);
	std::string part;
	while (std::getline(ss, part, '/'))
	{
		if (part.empty() || part == ".") continue;
		if (part == "..") { cur = cur.parent_path(); continue; }
		fs::path next;
		if (!FindNoCase(cur, part, next)) return false;
		cur = next;
	}
	out = cur;
	std::error_code ec;
	return fs::is_regular_file(cur, ec);
}

std::string ResolveAssetPath(const std::string& baseDir, std::string name)
{
	for (char& c : name) if (c == '\\') c = '/';
	while (!name.empty() && std::isspace((unsigned char)name.back())) name.pop_back();
	size_t st = 0;
	while (st < name.size() && std::isspace((unsigned char)name[st])) st++;
	name.erase(0, st);
	if (name.empty()) return baseDir;

	const std::string original = baseDir + name;
	std::error_code ec;
	fs::path base = baseDir.empty() ? fs::path(".") : fs::path(baseDir);
	fs::path found;

	// Ruta tal cual (relativa al OBJ) y variante sin distinguir mayusculas.
	bool winAbs = name.size() > 1 && name[1] == ':';
	if (!winAbs)
	{
		if (name[0] == '/') { if (fs::is_regular_file(name, ec)) return name; }
		else
		{
			if (fs::is_regular_file(base / name, ec)) return original;
			if (ResolveNoCase(base, name, found)) return found.string();
		}
	}

	// Ruta absoluta de otra maquina: usar solo el nombre de archivo.
	std::string file = name.substr(name.find_last_of('/') == std::string::npos ? 0 : name.find_last_of('/') + 1);
	if (ResolveNoCase(base, file, found)) return found.string();

	// Buscar por nombre de archivo en el directorio del modelo y subcarpetas.
	const std::string lfile = LowerStr(file);
	int visited = 0;
	for (fs::recursive_directory_iterator it(base, fs::directory_options::skip_permission_denied, ec), end;
		!ec && it != end && visited < 20000; it.increment(ec), ++visited)
		if (it->is_regular_file(ec) && LowerStr(it->path().filename().string()) == lfile)
			return it->path().string();

	return original;
}
} // namespace


// OBJ File string indentifiers
#define VERTEX_ID		 "v"
#define TEXCOORD_ID		 "vt"
#define NORMAL_ID		 "vn"
#define FACE_ID			 "f"
#define COMMENT_ID		 "#"
#define MTL_LIB_ID		 "mtllib"
#define USE_MTL_ID		 "usemtl"

// MTL File string indentifiers
#define NEW_MTL_ID		 "newmtl"
#define MTL_TEXTURE_ID   "map_Kd"
#define MTL_AMBIENT_ID	 "Ka"
#define MTL_DIFFUSE_ID	 "Kd"
#define MTL_SPECULAR_ID	 "Ks"
#define MTL_SHININESS_ID "Ns"

// Maximum size of a string that could be read out of the OBJ file
#define MAX_STR_SIZE 1024

// Maximum number of vertices a that a single face can have
#define MAX_VERTICES 256

// Axis constants
const short unsigned int x = 0;
const short unsigned int y = 1;
const short unsigned int z = 2;

//////////////////////////////////////////////////////////////////////
// Construcktion / Destrucktion
//////////////////////////////////////////////////////////////////////

_stdcall COBJModel::COBJModel()
{
// Inicialitzar la llista de VAO's.
	initVAOList_OBJ();
}

_stdcall COBJModel::~COBJModel()
{
// Eliminar els VAO's de la llista.
	netejaVAOList_OBJ();
}

//bool _stdcall COBJModel::LoadModel(const char szFileName[],unsigned int iDisplayList)
//GLuint _stdcall COBJModel::LoadModel(char *szFileName, int prim_Id)
int _stdcall COBJModel::LoadModel(char* szFileName)
{
////////////////////////////////////////////////////////////////////////
// Load a OBJ file and render its data into a display list
////////////////////////////////////////////////////////////////////////
	OBJFileInfo OBJInfo;		  // General informations about the model
	OBJFileInfo CurrentIndex;	  // Current array index
	char szString[MAX_STR_SIZE];  // Buffer string for reading the file 
	char szBasePath[_MAX_PATH];	  // Path were all paths in the OBJ start
	int nScanReturn = 0;		  // Return value of fscanf
	float fCoord = 0;			  // Buffer float for reading the file
	int i;						  // Loop variable
	unsigned int iCurMaterial = 0;// Current material

	FILE *hFile=NULL;

// VAO
// Inicialitzar la llista de VAO's.
	initVAOList_OBJ();

// Get base path
	if (strcpy_s(szBasePath, szFileName) != 0)
		return ERANGE;
	MakePath(szBasePath);

////////////////////////////////////////////////////////////////////////
// Open the OBJ file
////////////////////////////////////////////////////////////////////////
	errno = 0;
	errno = fopen_s(&hFile, szFileName, "r"); // Funci� Visual 2005 i 2010
	
// Success ?
	if (errno != 0)
		return errno; //FALSE;

////////////////////////////////////////////////////////////////////////
// Allocate space for structures that hold the model data
////////////////////////////////////////////////////////////////////////
 
// Which data types are stored in the file ? How many of each type ?
	GetFileInfo(hFile, &OBJInfo, szBasePath);
	if (OBJInfo.iMaterialCount > MAX_SIZE_VAOLIST)
	{
		fclose(hFile);
		return EOVERFLOW;
	}

// Vertices and faces
	Vector3D	*pVertices	= new Vector3D[OBJInfo.iVertexCount];
	Face		*pFaces		= new Face[OBJInfo.iFaceCount];
	
// Allocate space for optional model data only if present.
	Vector3D	*pNormals	= 0;
	Vector2D	*pTexCoords = 0;
	Material	*pMaterials	= 0;
	if (OBJInfo.iNormalCount)
		pNormals = new Vector3D[OBJInfo.iNormalCount];
	if (OBJInfo.iTexCoordCount)
		pTexCoords = new Vector2D[OBJInfo.iTexCoordCount];
	if (OBJInfo.iMaterialCount)
		pMaterials = new Material[OBJInfo.iMaterialCount]{};

	// Init structure that holds the current array index
	memset(&CurrentIndex, 0, sizeof(OBJFileInfo));
	auto failLoad = [&](int errorCode)
	{
		fclose(hFile);
		for (unsigned int faceIndex = 0; faceIndex < CurrentIndex.iFaceCount; ++faceIndex)
		{
			delete[] pFaces[faceIndex].pNormals;
			delete[] pFaces[faceIndex].pTexCoords;
			delete[] pFaces[faceIndex].pVertices;
		}
		delete[] pFaces;
		delete[] pVertices;
		delete[] pNormals;
		delete[] pTexCoords;
		if (pMaterials)
		{
			for (unsigned int materialIndex = 0; materialIndex < OBJInfo.iMaterialCount; ++materialIndex)
				if (pMaterials[materialIndex].iTextureID != 0)
					glDeleteTextures(1, &pMaterials[materialIndex].iTextureID);
		}
		delete[] pMaterials;
		return errorCode;
	};

////////////////////////////////////////////////////////////////////////
// Read the file contents
////////////////////////////////////////////////////////////////////////

// Start reading the file from the start
	rewind(hFile);
	
// Quit reading when end of file has been reached
	while (ReadNextString(szString, hFile))
	{
		// Next three elements are floats of a vertex
		if (!strncmp(szString, VERTEX_ID, sizeof(VERTEX_ID)))
		{
			if (CurrentIndex.iVertexCount >= OBJInfo.iVertexCount)
				return failLoad(EINVAL);
			// Read three floats out of the file
			nScanReturn = fscanf_s(hFile, "%f %f %f",
				&pVertices[CurrentIndex.iVertexCount].fX,
				&pVertices[CurrentIndex.iVertexCount].fY,
				&pVertices[CurrentIndex.iVertexCount].fZ);
			if (nScanReturn != 3)
				return failLoad(EINVAL);
			// Next vertex
			CurrentIndex.iVertexCount++;
		}

		// Next two elements are floats of a texture coordinate
		if (!strncmp(szString, TEXCOORD_ID, sizeof(TEXCOORD_ID)))
		{
			if (!pTexCoords || CurrentIndex.iTexCoordCount >= OBJInfo.iTexCoordCount)
				return failLoad(EINVAL);
			// Read two floats out of the file
			nScanReturn = fscanf_s(hFile, "%f %f",
				&pTexCoords[CurrentIndex.iTexCoordCount].fX,
				&pTexCoords[CurrentIndex.iTexCoordCount].fY);
			if (nScanReturn != 2)
				return failLoad(EINVAL);
			// Next texture coordinate
			CurrentIndex.iTexCoordCount++;
		}

		// Next three elements are floats of a vertex normal
		if (!strncmp(szString, NORMAL_ID, sizeof(NORMAL_ID)))
		{
			if (!pNormals || CurrentIndex.iNormalCount >= OBJInfo.iNormalCount)
				return failLoad(EINVAL);
			// Read three floats out of the file
			nScanReturn = fscanf_s(hFile, "%f %f %f",
				&pNormals[CurrentIndex.iNormalCount].fX,
				&pNormals[CurrentIndex.iNormalCount].fY,
				&pNormals[CurrentIndex.iNormalCount].fZ);
			if (nScanReturn != 3)
				return failLoad(EINVAL);
			// Next normal
			CurrentIndex.iNormalCount++;
		}

		// Rest of the line contains face information
		if (!strncmp(szString, FACE_ID, sizeof(FACE_ID)))
		{	// Read the rest of the line (the complete face)
			if (CurrentIndex.iFaceCount >= OBJInfo.iFaceCount)
				return failLoad(EINVAL);
			GetTokenParameter(szString, sizeof(szString) ,hFile);
			// Convert string into a face structure
			if (!ParseFaceString(szString, &pFaces[CurrentIndex.iFaceCount], pVertices,
				OBJInfo.iVertexCount, pNormals, OBJInfo.iNormalCount,
				pTexCoords, OBJInfo.iTexCoordCount, iCurMaterial))
				return failLoad(EINVAL);
			// Next face
			CurrentIndex.iFaceCount++;
		}

		// Process material information only if needed
		if (pMaterials)
		{	// Rest of the line contains the name of a material
			if (!strncmp(szString, USE_MTL_ID, sizeof(USE_MTL_ID)))
			{	// Read the rest of the line (the complete material name)
				GetTokenParameter(szString, sizeof(szString), hFile);
				// Are any materials loaded ?
				if (pMaterials)
					// Find material array index for the material name
					for (i=0; i<(int) OBJInfo.iMaterialCount; i++)
						if (!strncmp(pMaterials[i].szName, szString, sizeof(szString)))
						{	iCurMaterial = i;
							break;
						}
			}

			// Rest of the line contains the filename of a material library
			if (!strncmp(szString, MTL_LIB_ID, sizeof(MTL_LIB_ID)))
			{
				// Read the rest of the line (the complete filename)
				GetTokenParameter(szString, sizeof(szString), hFile);
				// Append material library filename to the model's base path
				char szLibraryFile[_MAX_PATH];
				{
					std::string resolved = ResolveAssetPath(szBasePath, szString);
					strcpy_s(szLibraryFile, resolved.c_str());
				}
				// Load the material library
				LoadMaterialLib(szLibraryFile, pMaterials, 
					&CurrentIndex.iMaterialCount, szBasePath);
			}
		}
	}	

	// Close OBJ file
	fclose(hFile);

////////////////////////////////////////////////////////////////////////
// Arrange and render the model data
////////////////////////////////////////////////////////////////////////

// Sort the faces by material to minimize state changes
	if (pMaterials)
	{	qsort(pFaces, OBJInfo.iFaceCount, sizeof(Face), CompareFaceByMaterial);

		// Copiar el vector pMaterials a un vector fixe vMaterials.
		materialCount = OBJInfo.iMaterialCount;
		for (unsigned int i = 0; i < materialCount; ++i)
			vMaterials[i] = pMaterials[i];
	}
	else
		materialCount = 0;

// Render all faces into a VAO
	//auxVAO = RenderToVAOList(pFaces, OBJInfo.iFaceCount, pMaterials, prim_Id);
	loadToVAOList(pFaces, OBJInfo.iFaceCount, pMaterials);

////////////////////////////////////////////////////////////////////////
// Free structures that hold the model data
////////////////////////////////////////////////////////////////////////

// Remove vertices, normals, materials and texture coordinates
	delete [] pVertices;	pVertices = 0;
	delete [] pNormals;		pNormals = 0;
	delete [] pTexCoords;	pTexCoords = 0;
	delete [] pMaterials;	pMaterials = 0;

// Remove face array
	for (i=0; i<(int) OBJInfo.iFaceCount; i++)
	{
		// Delete every pointer in the face structure
		delete [] pFaces[i].pNormals;	pFaces[i].pNormals = 0;
		delete [] pFaces[i].pTexCoords;	pFaces[i].pTexCoords = 0;
		delete [] pFaces[i].pVertices;  pFaces[i].pVertices = 0;
	}
	delete [] pFaces;
	pFaces = 0;
	
////////////////////////////////////////////////////////////////////////
// Success
////////////////////////////////////////////////////////////////////////
	return 0;
}

bool _stdcall COBJModel::ParseFaceString(char szFaceString[], Face *FaceOut,
								const Vector3D *pVertices, unsigned int vertexCount,
								const Vector3D *pNormals, unsigned int normalCount,
								const Vector2D *pTexCoords, unsigned int texCoordCount,
								const unsigned int iMaterialIndex)
{
	memset(FaceOut, 0, sizeof(Face));
	int vertexIndices[MAX_VERTICES]{};
	int texCoordIndices[MAX_VERTICES]{};
	int normalIndices[MAX_VERTICES]{};
	unsigned int faceVertexCount = 0;
	char* cursor = szFaceString;

	while (*cursor != '\0')
	{
		while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')
			++cursor;
		if (*cursor == '\0' || *cursor == '#') break;
		if (faceVertexCount >= MAX_VERTICES) return false;

		char* tokenEnd = cursor;
		while (*tokenEnd != '\0' && *tokenEnd != ' ' && *tokenEnd != '\t' &&
			*tokenEnd != '\r' && *tokenEnd != '\n')
			++tokenEnd;
		const char savedEnd = *tokenEnd;
		*tokenEnd = '\0';

		char* indexCursor = cursor;
		auto readIndex = [&indexCursor](int& index)
		{
			char* end = nullptr;
			errno = 0;
			const long parsed = std::strtol(indexCursor, &end, 10);
			if (end == indexCursor || errno == ERANGE || parsed < INT_MIN || parsed > INT_MAX)
				return false;
			index = static_cast<int>(parsed);
			indexCursor = end;
			return true;
		};
		bool validToken = readIndex(vertexIndices[faceVertexCount]);
		if (validToken && pNormals && pTexCoords)
		{
			validToken = *indexCursor == '/';
			if (validToken) ++indexCursor;
			validToken = validToken && readIndex(texCoordIndices[faceVertexCount]);
			validToken = validToken && *indexCursor == '/';
			if (validToken) ++indexCursor;
			validToken = validToken && readIndex(normalIndices[faceVertexCount]);
		}
		else if (validToken && pNormals)
		{
			validToken = indexCursor[0] == '/' && indexCursor[1] == '/';
			if (validToken) indexCursor += 2;
			validToken = validToken && readIndex(normalIndices[faceVertexCount]);
		}
		else if (validToken && pTexCoords)
		{
			validToken = *indexCursor == '/';
			if (validToken) ++indexCursor;
			validToken = validToken && readIndex(texCoordIndices[faceVertexCount]);
		}
		validToken = validToken && *indexCursor == '\0';
		*tokenEnd = savedEnd;
		if (!validToken) return false;
		if (vertexIndices[faceVertexCount] <= 0 ||
			static_cast<unsigned int>(vertexIndices[faceVertexCount]) > vertexCount)
			return false;
		if (pTexCoords && (texCoordIndices[faceVertexCount] <= 0 ||
			static_cast<unsigned int>(texCoordIndices[faceVertexCount]) > texCoordCount))
			return false;
		if (pNormals && (normalIndices[faceVertexCount] <= 0 ||
			static_cast<unsigned int>(normalIndices[faceVertexCount]) > normalCount))
			return false;

		++faceVertexCount;
		cursor = tokenEnd;
	}
	if (faceVertexCount < 3) return false;

	FaceOut->iNumVertices = faceVertexCount;
	FaceOut->iMaterialIndex = iMaterialIndex;
	FaceOut->pVertices = new Vector3D[faceVertexCount];
	if (pNormals) FaceOut->pNormals = new Vector3D[faceVertexCount];
	if (pTexCoords) FaceOut->pTexCoords = new Vector2D[faceVertexCount];

	for (unsigned int i = 0; i < faceVertexCount; ++i)
	{
		FaceOut->pVertices[i] = pVertices[vertexIndices[i] - 1];
		if (pNormals) FaceOut->pNormals[i] = pNormals[normalIndices[i] - 1];
		if (pTexCoords) FaceOut->pTexCoords[i] = pTexCoords[texCoordIndices[i] - 1];
	}
	return true;
}

bool _stdcall COBJModel::LoadMaterialLib(const char szFileName[], 
								Material *pMaterials, 
								unsigned int *iCurMaterialIndex,
								char szBasePath[])
{
////////////////////////////////////////////////////////////////////////
// Loads a material library file (.mtl)
////////////////////////////////////////////////////////////////////////

	char szString[MAX_STR_SIZE];	// Buffer used while reading the file
	bool bFirstMaterial = TRUE;		// Only increase index after first 
									// material has been set
	FILE *hFileT = NULL;
	int err;

////////////////////////////////////////////////////////////////////////
// Open library file
////////////////////////////////////////////////////////////////////////
	err = fopen_s(&hFileT,szFileName, "r");
	// Success ?
	if (err!=0) //(!hFile)
		return FALSE;

////////////////////////////////////////////////////////////////////////
// Read all material definitions
////////////////////////////////////////////////////////////////////////

// Quit reading when end of file has been reached
	while (ReadNextString(szString, hFileT))
	{
		// Is it a "new material" identifier ?
		if (!strncmp(szString, NEW_MTL_ID, sizeof(NEW_MTL_ID)))
		{	// Only increase index after first material has been set
			if (bFirstMaterial == TRUE)
				// First material has been set
				bFirstMaterial = FALSE;
			else
				// Use next index
				(*iCurMaterialIndex)++;
			// Read material name
			GetTokenParameter(szString, sizeof(szString), hFileT);
			// Store material name in the structure
			strcpy_s(pMaterials[*iCurMaterialIndex].szName, szString);
			// Inicialitzar valors registre pMaterials
			pMaterials[*iCurMaterialIndex].iTextureID = 0;
			pMaterials[*iCurMaterialIndex].fEmmissive[0] = 0.0f;	pMaterials[*iCurMaterialIndex].fEmmissive[1] = 0.0f;	pMaterials[*iCurMaterialIndex].fEmmissive[2] = 0.0f;
			pMaterials[*iCurMaterialIndex].fAmbient[0] = 0.0f;		pMaterials[*iCurMaterialIndex].fAmbient[1] = 0.0f;		pMaterials[*iCurMaterialIndex].fAmbient[2] = 0.0f;
			pMaterials[*iCurMaterialIndex].fDiffuse[0] = 0.0f;		pMaterials[*iCurMaterialIndex].fDiffuse[1] = 0.0f;		pMaterials[*iCurMaterialIndex].fDiffuse[2] = 0.0f;
			pMaterials[*iCurMaterialIndex].fAmbient[0] = 0.0f;		pMaterials[*iCurMaterialIndex].fAmbient[1] = 0.0f;		pMaterials[*iCurMaterialIndex].fAmbient[2] = 0.0f;
			pMaterials[*iCurMaterialIndex].fSpecular[0] = 0.0f;		pMaterials[*iCurMaterialIndex].fSpecular[1] = 0.0f;		pMaterials[*iCurMaterialIndex].fSpecular[2] = 0.0f;
			pMaterials[*iCurMaterialIndex].fShininess = 0.0f;
		}

		// Ambient material properties
		if (!strncmp(szString, MTL_AMBIENT_ID, sizeof(MTL_AMBIENT_ID)))
		{	// Read into current material
			fscanf(hFileT, "%f %f %f",
				&pMaterials[*iCurMaterialIndex].fAmbient[0],
				&pMaterials[*iCurMaterialIndex].fAmbient[1],
				&pMaterials[*iCurMaterialIndex].fAmbient[2]);
		}

// Diffuse material properties
		if (!strncmp(szString, MTL_DIFFUSE_ID, sizeof(MTL_DIFFUSE_ID)))
		{	// Read into current material
			fscanf(hFileT, "%f %f %f",
				&pMaterials[*iCurMaterialIndex].fDiffuse[0],
				&pMaterials[*iCurMaterialIndex].fDiffuse[1],
				&pMaterials[*iCurMaterialIndex].fDiffuse[2]);
		}

// Specular material properties
		if (!strncmp(szString, MTL_SPECULAR_ID, sizeof(MTL_SPECULAR_ID)))
		{	// Read into current material
			fscanf(hFileT, "%f %f %f",
				&pMaterials[*iCurMaterialIndex].fSpecular[0],
				&pMaterials[*iCurMaterialIndex].fSpecular[1],
				&pMaterials[*iCurMaterialIndex].fSpecular[2]);
		}

// Texture map name
		if (!strncmp(szString, MTL_TEXTURE_ID, sizeof(MTL_TEXTURE_ID)))
		{	// Read texture filename
			GetTokenParameter(szString, sizeof(szString), hFileT);
			// Append material library filename to the model's base path
			char szTextureFile[_MAX_PATH];
			{
				std::string resolved = ResolveAssetPath(szBasePath, szString);
				strcpy_s(szTextureFile, resolved.c_str());
			}
			// Store texture filename in the structure
			strcpy_s(pMaterials[*iCurMaterialIndex].szTexture, szTextureFile);
			// Load texture and store its ID in the structure
			pMaterials[*iCurMaterialIndex].iTextureID = LoadTexture2(szTextureFile);
		}

// Shininess
		if (!strncmp(szString, MTL_SHININESS_ID, sizeof(MTL_SHININESS_ID)))
		{	// Read into current material
			fscanf(hFileT, "%f",
				&pMaterials[*iCurMaterialIndex].fShininess);
			// OBJ files use a shininess from 0 to 1000; Scale for OpenGL
			pMaterials[*iCurMaterialIndex].fShininess /= 1000.0f;
			pMaterials[*iCurMaterialIndex].fShininess *= 128.0f;
		}
	}

	fclose(hFileT);

// Increment index cause LoadMaterialLib() assumes that the passed
// index is always empty
	(*iCurMaterialIndex)++;

	materialCount = *iCurMaterialIndex;
	return TRUE;
}

CVAO _stdcall COBJModel::RenderToVAOList(const Face* pFaces,
									const unsigned int iFaceCount,
									const Material *pMaterials)
{
////////////////////////////////////////////////////////////////////////
// Render a list of faces into a VAO
////////////////////////////////////////////////////////////////////////

	int i, j;
	float fNormal[3];
	int iPreviousMaterial = -1;
	double color[4] = { 1.0F, 0.0f, 0.0f, 1.0f };

// VAO
	//GLuint vaoId = 0;		GLuint vboId = 0;
	CVAO objVAO = {0,0,0,0,0};

	objVAO.vaoId = 0;	objVAO.vboId = 0;	objVAO.eboId = 0; objVAO.nVertexs = 0; objVAO.nIndices = 0;

	std::vector <double> vertices, colors, normals, textures;		// Definici� vectors din�mics per a vertexs i colors 
	vertices.resize(0);		colors.resize(0);	normals.resize(0);		textures.resize(0);// Reinicialitzar vectors
	
// Legacy current-color state is unavailable in a core-profile context.
const GLfloat cColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

// Generate & save display list index
	
// Render model into the display list
	//glNewList(m_iDisplayList, GL_COMPILE);

			// Activate automatic texture coord generation if no coords loaded
			if (!pFaces[0].pTexCoords)
				GenTexCoords();
		
			// Use default material if no materials are loaded
			if (!pMaterials)
				UseMaterial(NULL);

			// Process all faces
			for (i=0; i<(int) iFaceCount; i++)
			{	// Process all vertices as triangles
				for (j = 1; j < (int)pFaces[i].iNumVertices-1; j++)
				{	// -----------VERTEX 0
					// Set vertex normal (if vertex normals are specified)
					if (pFaces[i].pNormals)
						{	//glNormal3f(pFaces[i].pNormals[j].fX, pFaces[i].pNormals[j].fY, pFaces[i].pNormals[j].fZ);
							normals.push_back(pFaces[i].pNormals[0].fX);
							normals.push_back(pFaces[i].pNormals[0].fY);
							normals.push_back(pFaces[i].pNormals[0].fZ);
						}

					else {	// Calculate and set face normal if no vertex normals are specified
							GetFaceNormal(fNormal, &pFaces[i]);
							//glNormal3fv(fNormal);
							normals.push_back(fNormal[0]);
							normals.push_back(fNormal[1]);
							normals.push_back(fNormal[2]);
						}

					// Set texture coordinates (if any specified)
					if (pMaterials)
					{	if (pMaterials[pFaces[i].iMaterialIndex].iTextureID != 0)
							{	if (pFaces[i].pTexCoords)
									{	// glTexCoord2f(pFaces[i].pTexCoords[j].fX, pFaces[i].pTexCoords[j].fY);
										textures.push_back(pFaces[i].pTexCoords[0].fX);	
										textures.push_back(pFaces[i].pTexCoords[0].fY);
									}
									else {	textures.push_back(0.0); // textures.push_back(-0.1*pFaces[i].pVertices[j].fX);
											textures.push_back(0.0); // textures.push_back(-0.1 * pFaces[i].pVertices[j].fZ);
										}
							}
							else {	textures.push_back(0.0);
									textures.push_back(0.0);
								}
					}
					else {	textures.push_back(0.0);
							textures.push_back(0.0);
						}

					// Set COLOR: Any materials loaded ?
					if (pMaterials)
					{	// Set material (if it differs from the previous one)
						if (iPreviousMaterial != (int)pFaces[i].iMaterialIndex)
						{	iPreviousMaterial = pFaces[i].iMaterialIndex;
							UseMaterial(&pMaterials[pFaces[i].iMaterialIndex]);
						}
						// Set Color
						colors.push_back(pMaterials->fDiffuse[0]);
						colors.push_back(pMaterials->fDiffuse[1]);
						colors.push_back(pMaterials->fDiffuse[2]);
						colors.push_back(1.0);
					}
					else {	// Set Color per defecte
						colors.push_back(cColor[0]);
						colors.push_back(cColor[1]);
						colors.push_back(cColor[2]);
						colors.push_back(cColor[3]);
					}

					// Set VERTEX
					//glVertex3f(pFaces[i].pVertices[j].fX, pFaces[i].pVertices[j].fY, pFaces[i].pVertices[j].fZ);
					vertices.push_back(pFaces[i].pVertices[0].fX);
					vertices.push_back(pFaces[i].pVertices[0].fY);
					vertices.push_back(pFaces[i].pVertices[0].fZ);

					// -----------VERTEX j
// Set vertex normal (if vertex normals are specified)
					if (pFaces[i].pNormals)
					{	//glNormal3f(pFaces[i].pNormals[j].fX, pFaces[i].pNormals[j].fY, pFaces[i].pNormals[j].fZ);
						normals.push_back(pFaces[i].pNormals[j].fX);
						normals.push_back(pFaces[i].pNormals[j].fY);
						normals.push_back(pFaces[i].pNormals[j].fZ);
					}

					else {	// Calculate and set face normal if no vertex normals are specified
						GetFaceNormal(fNormal, &pFaces[i]);
						//glNormal3fv(fNormal);
						normals.push_back(fNormal[0]);
						normals.push_back(fNormal[1]);
						normals.push_back(fNormal[2]);
					}

					// Set texture coordinates (if any specified)
					if (pMaterials)
					{	if (pMaterials[pFaces[i].iMaterialIndex].iTextureID != 0)
						{	if (pFaces[i].pTexCoords)
							{	// glTexCoord2f(pFaces[i].pTexCoords[j].fX, pFaces[i].pTexCoords[j].fY);
								textures.push_back(pFaces[i].pTexCoords[j].fX);
								textures.push_back(pFaces[i].pTexCoords[j].fY);
							}
							else {	textures.push_back(0.0); // textures.push_back(-0.1*pFaces[i].pVertices[j].fX);
									textures.push_back(0.0); // textures.push_back(-0.1 * pFaces[i].pVertices[j].fZ);
								}
						}
						else {	textures.push_back(0.0);
								textures.push_back(0.0);
							}
					}
					else {	textures.push_back(0.0);
							textures.push_back(0.0);
						}

					// Set COLOR: Any materials loaded ?
					if (pMaterials)
					{	// Set material (if it differs from the previous one)
						if (iPreviousMaterial != (int)pFaces[i].iMaterialIndex)
						{	iPreviousMaterial = pFaces[i].iMaterialIndex;
							UseMaterial(&pMaterials[pFaces[i].iMaterialIndex]);
						}
						// Set Color
						colors.push_back(pMaterials->fDiffuse[0]);
						colors.push_back(pMaterials->fDiffuse[1]);
						colors.push_back(pMaterials->fDiffuse[2]);
						colors.push_back(1.0);
					}
					else {	// Set Color per defecte
						colors.push_back(cColor[0]);
						colors.push_back(cColor[1]);
						colors.push_back(cColor[2]);
						colors.push_back(cColor[3]);
					}

					// Set VERTEX
					//glVertex3f(pFaces[i].pVertices[j].fX, pFaces[i].pVertices[j].fY, pFaces[i].pVertices[j].fZ);
					vertices.push_back(pFaces[i].pVertices[j].fX);
					vertices.push_back(pFaces[i].pVertices[j].fY);
					vertices.push_back(pFaces[i].pVertices[j].fZ);


					// -----------VERTEX j+1
// Set vertex normal (if vertex normals are specified)
					if (pFaces[i].pNormals)
					{	//glNormal3f(pFaces[i].pNormals[j].fX, pFaces[i].pNormals[j].fY, pFaces[i].pNormals[j].fZ);
						normals.push_back(pFaces[i].pNormals[j+1].fX);
						normals.push_back(pFaces[i].pNormals[j+1].fY);
						normals.push_back(pFaces[i].pNormals[j+1].fZ);
					}

					else {	// Calculate and set face normal if no vertex normals are specified
						GetFaceNormal(fNormal, &pFaces[i]);
						//glNormal3fv(fNormal);
						normals.push_back(fNormal[0]);
						normals.push_back(fNormal[1]);
						normals.push_back(fNormal[2]);
					}

					// Set texture coordinates (if any specified)
					if (pMaterials)
					{	if (pMaterials[pFaces[i].iMaterialIndex].iTextureID != 0)
						{	if (pFaces[i].pTexCoords)
							{	// glTexCoord2f(pFaces[i].pTexCoords[j].fX, pFaces[i].pTexCoords[j].fY);
								textures.push_back(pFaces[i].pTexCoords[j+1].fX);
								textures.push_back(pFaces[i].pTexCoords[j+1].fY);
							}
							else {	textures.push_back(0.0); // textures.push_back(-0.1*pFaces[i].pVertices[j].fX);
									textures.push_back(0.0); // textures.push_back(-0.1 * pFaces[i].pVertices[j].fZ);
								}
						}
						else {	textures.push_back(0.0);
								textures.push_back(0.0);
							}
					}
					else {	textures.push_back(0.0);
							textures.push_back(0.0);
						}

					// Set COLOR: Any materials loaded ?
					if (pMaterials)
					{	// Set material (if it differs from the previous one)
						if (iPreviousMaterial != (int)pFaces[i].iMaterialIndex)
						{	iPreviousMaterial = pFaces[i].iMaterialIndex;
							UseMaterial(&pMaterials[pFaces[i].iMaterialIndex]);
						}
						// Set Color
						colors.push_back(pMaterials->fDiffuse[0]);
						colors.push_back(pMaterials->fDiffuse[1]);
						colors.push_back(pMaterials->fDiffuse[2]);
						colors.push_back(1.0);
					}
					else {	// Set Color per defecte
						colors.push_back(cColor[0]);
						colors.push_back(cColor[1]);
						colors.push_back(cColor[2]);
						colors.push_back(cColor[3]);
					}

					// Set VERTEX
					//glVertex3f(pFaces[i].pVertices[j].fX, pFaces[i].pVertices[j].fY, pFaces[i].pVertices[j].fZ);
					vertices.push_back(pFaces[i].pVertices[j+1].fX);
					vertices.push_back(pFaces[i].pVertices[j+1].fY);
					vertices.push_back(pFaces[i].pVertices[j+1].fZ);
				}
			}
	//glEndList();

// ----------------------- VAO
// Creaci� d'un VAO i un VBO i c�rrega de la geometria. Guardar identificador VAO identificador VBO a struct CVAO.
	objVAO = load_TRIANGLES_VAO(vertices, normals, colors, textures);

	return objVAO;
}


void _stdcall COBJModel::loadToVAOList(const Face* pFaces,
	const unsigned int iFaceCount,
	const Material* pMaterials)
{
////////////////////////////////////////////////////////////////////////
// Load a list of faces into a VAO
////////////////////////////////////////////////////////////////////////
	int i, j;
	float fNormal[3];
	int iPreviousMaterial = -1;
	double color[4] = { 1.0F, 0.0f, 0.0f, 1.0f };

// VAO
	int index_VAO = 0;
	CVAO objVAO;
	objVAO.vaoId = 0;	objVAO.vboId = 0;	objVAO.eboId = 0;	 objVAO.nVertexs = 0; 	objVAO.nIndices = 0;

	std::vector <double> vertices, colors, normals, textures;		// Definici� vectors din�mics per a vertexs i colors 
	vertices.resize(0);		colors.resize(0);	normals.resize(0);		textures.resize(0);// Reinicialitzar vectors

	std::vector <int>::size_type nv = vertices.size();	// Tamany del vector vertices en elements.

	netejaVAOList_OBJ();

// Core profiles have no fixed-function current color state.
	const GLfloat cColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

// Activate automatic texture coord generation if no coords loaded
	if (!pFaces[0].pTexCoords) GenTexCoords();

// Use default material if no materials are loaded
	if (!pMaterials) UseMaterial(NULL);

// Process all faces
	for (i = 0; i < (int)iFaceCount; i++)
	{	// Process all vertices as triangles
		for (j = 1; j < (int)pFaces[i].iNumVertices - 1; j++)
		{	nv = vertices.size();	// Tamany del vector vertices en elements.
			if (pMaterials)
			{	// Set material (if it differs from the previous one)
				if (iPreviousMaterial != (int)pFaces[i].iMaterialIndex)
				{	// Canvi de material per a les cares del fitxer OBJ
					if (nv > 0) {
						// Creaci� d'un VAO i un VBO i c�rrega de la geometria. Guardar identificador VAO identificador VBO a struct CVAO.
						objVAO = load_TRIANGLES_VAO(vertices, normals, colors, textures);
						Set_VAOList_OBJ(index_VAO, objVAO);
						index_VAO = index_VAO + 1;
						vertices.resize(0);		colors.resize(0);	normals.resize(0);		textures.resize(0);// Reinicialitzar vectors
						//iPreviousMaterial = pFaces[i].iMaterialIndex;
						//UseMaterial(&pMaterials[pFaces[i].iMaterialIndex]);
					}
					vector_Materials[index_VAO] = pFaces[i].iMaterialIndex;
					iPreviousMaterial = pFaces[i].iMaterialIndex;
					//UseMaterial(&pMaterials[pFaces[i].iMaterialIndex]);
				}
			}
			// -----------VERTEX 0
			// Set vertex normal (if vertex normals are specified)
			if (pFaces[i].pNormals)
			{	//glNormal3f(pFaces[i].pNormals[j].fX, pFaces[i].pNormals[j].fY, pFaces[i].pNormals[j].fZ);
				normals.push_back(pFaces[i].pNormals[0].fX);
				normals.push_back(pFaces[i].pNormals[0].fY);
				normals.push_back(pFaces[i].pNormals[0].fZ);
			}

			else {	// Calculate and set face normal if no vertex normals are specified
				GetFaceNormal(fNormal, &pFaces[i]);
				//glNormal3fv(fNormal);
				normals.push_back(fNormal[0]);
				normals.push_back(fNormal[1]);
				normals.push_back(fNormal[2]);
			}

			// Set texture coordinates (if any specified)
			if (pMaterials)
			{	if (pMaterials[pFaces[i].iMaterialIndex].iTextureID != 0)
				{	if (pFaces[i].pTexCoords)
					{	// glTexCoord2f(pFaces[i].pTexCoords[j].fX, pFaces[i].pTexCoords[j].fY);
						textures.push_back(pFaces[i].pTexCoords[0].fX);
						textures.push_back(pFaces[i].pTexCoords[0].fY);
					}
					else {	textures.push_back(0.0); // textures.push_back(-0.1*pFaces[i].pVertices[j].fX);
							textures.push_back(0.0); // textures.push_back(-0.1 * pFaces[i].pVertices[j].fZ);
						}
				}
				else {	textures.push_back(0.0);
						textures.push_back(0.0);
					}
			}
			else {	textures.push_back(0.0);
					textures.push_back(0.0);
				}

			// Set COLOR: Any materials loaded ?
			if (pMaterials)
			{	// Set Color
				colors.push_back(pMaterials->fDiffuse[0]);
				colors.push_back(pMaterials->fDiffuse[1]);
				colors.push_back(pMaterials->fDiffuse[2]);
				colors.push_back(1.0);
			}
			else {	// Set Color per defecte
				colors.push_back(cColor[0]);
				colors.push_back(cColor[1]);
				colors.push_back(cColor[2]);
				colors.push_back(cColor[3]);
			}

			// Set VERTEX
			//glVertex3f(pFaces[i].pVertices[j].fX, pFaces[i].pVertices[j].fY, pFaces[i].pVertices[j].fZ);
			vertices.push_back(pFaces[i].pVertices[0].fX);
			vertices.push_back(pFaces[i].pVertices[0].fY);
			vertices.push_back(pFaces[i].pVertices[0].fZ);

			// -----------VERTEX j
// Set vertex normal (if vertex normals are specified)
			if (pFaces[i].pNormals)
			{	//glNormal3f(pFaces[i].pNormals[j].fX, pFaces[i].pNormals[j].fY, pFaces[i].pNormals[j].fZ);
				normals.push_back(pFaces[i].pNormals[j].fX);
				normals.push_back(pFaces[i].pNormals[j].fY);
				normals.push_back(pFaces[i].pNormals[j].fZ);
			}

			else {	// Calculate and set face normal if no vertex normals are specified
				GetFaceNormal(fNormal, &pFaces[i]);
				//glNormal3fv(fNormal);
				normals.push_back(fNormal[0]);
				normals.push_back(fNormal[1]);
				normals.push_back(fNormal[2]);
			}

			// Set texture coordinates (if any specified)
			if (pMaterials)
			{	if (pMaterials[pFaces[i].iMaterialIndex].iTextureID != 0)
				{	if (pFaces[i].pTexCoords)
					{	// glTexCoord2f(pFaces[i].pTexCoords[j].fX, pFaces[i].pTexCoords[j].fY);
						textures.push_back(pFaces[i].pTexCoords[j].fX);
						textures.push_back(pFaces[i].pTexCoords[j].fY);
					}
					else {	textures.push_back(0.0); // textures.push_back(-0.1*pFaces[i].pVertices[j].fX);
							textures.push_back(0.0); // textures.push_back(-0.1 * pFaces[i].pVertices[j].fZ);
						}
				}
				else {	textures.push_back(0.0);
						textures.push_back(0.0);
					}
			}
			else {	textures.push_back(0.0);
					textures.push_back(0.0);
				}

			// Set COLOR: Any materials loaded ?
			if (pMaterials)
			{	// Set Color
				colors.push_back(pMaterials->fDiffuse[0]);
				colors.push_back(pMaterials->fDiffuse[1]);
				colors.push_back(pMaterials->fDiffuse[2]);
				colors.push_back(1.0);
			}
			else {	// Set Color per defecte
				colors.push_back(cColor[0]);
				colors.push_back(cColor[1]);
				colors.push_back(cColor[2]);
				colors.push_back(cColor[3]);
			}

			// Set VERTEX
			//glVertex3f(pFaces[i].pVertices[j].fX, pFaces[i].pVertices[j].fY, pFaces[i].pVertices[j].fZ);
			vertices.push_back(pFaces[i].pVertices[j].fX);
			vertices.push_back(pFaces[i].pVertices[j].fY);
			vertices.push_back(pFaces[i].pVertices[j].fZ);

			// -----------VERTEX j+1
// Set vertex normal (if vertex normals are specified)
			if (pFaces[i].pNormals)
			{	//glNormal3f(pFaces[i].pNormals[j].fX, pFaces[i].pNormals[j].fY, pFaces[i].pNormals[j].fZ);
				normals.push_back(pFaces[i].pNormals[j + 1].fX);
				normals.push_back(pFaces[i].pNormals[j + 1].fY);
				normals.push_back(pFaces[i].pNormals[j + 1].fZ);
			}
			else {	// Calculate and set face normal if no vertex normals are specified
				GetFaceNormal(fNormal, &pFaces[i]);
				//glNormal3fv(fNormal);
				normals.push_back(fNormal[0]);
				normals.push_back(fNormal[1]);
				normals.push_back(fNormal[2]);
			}

			// Set texture coordinates (if any specified)
			if (pMaterials)
			{	if (pMaterials[pFaces[i].iMaterialIndex].iTextureID != 0)
				{	if (pFaces[i].pTexCoords)
					{	// glTexCoord2f(pFaces[i].pTexCoords[j].fX, pFaces[i].pTexCoords[j].fY);
						textures.push_back(pFaces[i].pTexCoords[j + 1].fX);
						textures.push_back(pFaces[i].pTexCoords[j + 1].fY);
					}
					else {	textures.push_back(0.0); // textures.push_back(-0.1*pFaces[i].pVertices[j].fX);
							textures.push_back(0.0); // textures.push_back(-0.1 * pFaces[i].pVertices[j].fZ);
						}
				}
				else {	textures.push_back(0.0);
						textures.push_back(0.0);
					}
			}
			else {	textures.push_back(0.0);
					textures.push_back(0.0);
				}

			// Set COLOR: Any materials loaded ?
			if (pMaterials)
			{	// Set Color
				colors.push_back(pMaterials->fDiffuse[0]);
				colors.push_back(pMaterials->fDiffuse[1]);
				colors.push_back(pMaterials->fDiffuse[2]);
				colors.push_back(1.0);
			}
			else {	// Set Color per defecte
				colors.push_back(cColor[0]);
				colors.push_back(cColor[1]);
				colors.push_back(cColor[2]);
				colors.push_back(cColor[3]);
			}

			// Set VERTEX
			//glVertex3f(pFaces[i].pVertices[j].fX, pFaces[i].pVertices[j].fY, pFaces[i].pVertices[j].fZ);
			vertices.push_back(pFaces[i].pVertices[j + 1].fX);
			vertices.push_back(pFaces[i].pVertices[j + 1].fY);
			vertices.push_back(pFaces[i].pVertices[j + 1].fZ);
		}
	}
	//glPopAttrib();
	//glEndList();

// ----------------------- VAO
	nv = vertices.size();	// Tamany del vector vertices en elements.

	if (nv != 0) 
	{	// Creaci� del darrer VAO i un VBO i c�rrega de la geometria. Guardar identificador VAO identificador VBO a struct CVAO.
		objVAO = load_TRIANGLES_VAO(vertices, normals, colors, textures);
		Set_VAOList_OBJ(index_VAO, objVAO);
	}

// Numero de Materials del fitxer OBJ
	numMaterials = index_VAO + 1;
}


void _stdcall COBJModel::UseMaterial(const Material *pMaterial)
{
	if (pMaterial != nullptr && pMaterial->iTextureID != 0)
		glBindTexture(GL_TEXTURE_2D, pMaterial->iTextureID);
	else
		glBindTexture(GL_TEXTURE_2D, 0);
}


void _stdcall COBJModel::UseMaterial_ShaderID(GLuint sh_programID, Material pMaterial)
{
////////////////////////////////////////////////////////////////////////
// Make a given material the current one
////////////////////////////////////////////////////////////////////////
	float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	//glColor3f(1.0,1.0,1.0);
// Look for the presence of a texture and activate texturing if succeed
	if (pMaterial.szName[0] != '\0')
	{
		//glUniform4i(glGetUniformLocation(sh_programID, "sw_intensity"), false, true, true, true);
		//glUniform1i(glGetUniformLocation(sh_programID, "sw_material"), true);

		if (pMaterial.iTextureID)
		{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, pMaterial.iTextureID);
			glUniform1i(glGetUniformLocation(sh_programID, "texture0"), GLint(0));
			glUniform1i(glGetUniformLocation(sh_programID, "textur"), GL_TRUE);
		}
		else {
			glUniform1i(glGetUniformLocation(sh_programID, "textur"), GL_FALSE);

			color[0] = pMaterial.fAmbient[0];	color[1] = pMaterial.fAmbient[1];	color[2] = pMaterial.fAmbient[2];
			glUniform4f(glGetUniformLocation(sh_programID, "material.ambient"), color[0], color[1], color[2], color[3]);

			color[0] = pMaterial.fDiffuse[0];	color[1] = pMaterial.fDiffuse[1];	color[2] = pMaterial.fDiffuse[2];
			glUniform4f(glGetUniformLocation(sh_programID, "material.diffuse"), color[0], color[1], color[2], color[3]);

			color[0] = pMaterial.fSpecular[0];	color[1] = pMaterial.fSpecular[1];	color[2] = pMaterial.fSpecular[2];
			glUniform4f(glGetUniformLocation(sh_programID, "material.specular"), color[0], color[1], color[2], color[3]);

			glUniform1f(glGetUniformLocation(sh_programID, "material.shininess"), pMaterial.fShininess);
		}
	}
	else
	{
		glUniform1i(glGetUniformLocation(sh_programID, "textur"), GL_FALSE);
	}
}


void _stdcall COBJModel::GenTexCoords()
{
////////////////////////////////////////////////////////////////////////
// Set up the automatic texture coord generation
////////////////////////////////////////////////////////////////////////

	float fS[4] = { -0.1f, 0.0f, 0.0f, 0.0f };
	float fT[4] = { 0.0f, 0.0f, -0.1f, 0.0f };

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexGenfv(GL_S, GL_OBJECT_PLANE, fS);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, fT);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);

	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
}

void _stdcall COBJModel::GetFaceNormal(float fNormalOut[], const Face *pFace)
{
////////////////////////////////////////////////////////////////////////
// Calculate normal for a given face
////////////////////////////////////////////////////////////////////////

// Only faces with at least 3 vertices can have a normal
	if (pFace->iNumVertices < 3)
	{
// Clear the normal and exit
		memset(fNormalOut, 0, sizeof(fNormalOut));
		return;
	}

// Two vectors
	float v1[3] = { 0.0,0.0,0.0 }, v2[3] = { 0.0,0.0,0.0 };

// Calculate two vectors from the three points
	v1[x] = pFace->pVertices[0].fX - pFace->pVertices[1].fX;
	v1[y] = pFace->pVertices[0].fY - pFace->pVertices[1].fY;
	v1[z] = pFace->pVertices[0].fZ - pFace->pVertices[1].fZ;

	v2[x] = pFace->pVertices[1].fX - pFace->pVertices[2].fX;
	v2[y] = pFace->pVertices[1].fY - pFace->pVertices[2].fY;
	v2[z] = pFace->pVertices[1].fZ - pFace->pVertices[2].fZ;

// Take the cross product of the two vectors to get the normal vector
	fNormalOut[x] = v1[y] * v2[z] - v1[z] * v2[y];
	fNormalOut[y] = v1[z] * v2[x] - v1[x] * v2[z];
	fNormalOut[z] = v1[x] * v2[y] - v1[y] * v2[x];

// Calculate the length of the vector		
	float length = (float) sqrt(( fNormalOut[x] * fNormalOut[x]) + 
						        ( fNormalOut[y] * fNormalOut[y]) +
						        ( fNormalOut[z] * fNormalOut[z]) );

// Keep the program from blowing up by providing an exceptable
// value for vectors that may calculated too close to zero.
	if(length == 0.0f)
		length = 1.0f;

// Dividing each element by the length will result in a
// unit normal vector.
	fNormalOut[x] /= length;
	fNormalOut[y] /= length;
	fNormalOut[z] /= length;
}

void _stdcall COBJModel::GetFileInfo(FILE *hStream, OBJFileInfo *Info, 
							const char szConstBasePath[])
{
////////////////////////////////////////////////////////////////////////
// Read the count of all important identifiers out of the given stream
////////////////////////////////////////////////////////////////////////

	char szString[MAX_STR_SIZE]; // Buffer for reading the file
	char szBasePath[_MAX_PATH];	 // Needed to append a filename to the base path
	
	FILE *hMaterialLib = NULL;
	int err;

// Init structure
	memset(Info, 0, sizeof(OBJFileInfo));

// Rollback the stream
	rewind(hStream);

// Quit reading when end of file has been reached
	while (ReadNextString(szString, hStream))
	{
// Vertex ?
		if (!strncmp(szString, VERTEX_ID, sizeof(VERTEX_ID)))
			Info->iVertexCount++;	
// Texture coordinate ?
		if (!strncmp(szString, TEXCOORD_ID, sizeof(TEXCOORD_ID)))
			Info->iTexCoordCount++;
		// Vertex normal ?
		if (!strncmp(szString, NORMAL_ID, sizeof(NORMAL_ID)))
			Info->iNormalCount++;		
		// Face ?
		if (!strncmp(szString, FACE_ID, sizeof(FACE_ID)))
			Info->iFaceCount++;

// Material library definition ?
		if (!strncmp(szString, MTL_LIB_ID, sizeof(MTL_LIB_ID)))
		{
			// Read the filename of the library
			GetTokenParameter(szString, sizeof(szString), hStream);
			// Copy the model's base path into a none-constant string
			{
				std::string resolved = ResolveAssetPath(szConstBasePath, szString);
				strcpy_s(szBasePath, resolved.c_str());
			}

			// Open the library file
//			FILE *hMaterialLib = fopen(szBasePath, "r");
			err = fopen_s(&hMaterialLib,szBasePath, "r");
			// Success ?
			if (!err) //(hMaterialLib)
			{
				// Quit reading when end of file has been reached
				while (fscanf(hMaterialLib, "%1023s", szString) == 1)
				{
					// Is it a "new material" identifier ?
					if (!strncmp(szString, NEW_MTL_ID, sizeof(NEW_MTL_ID)))
						// One more material defined
						Info->iMaterialCount++;		
				}
				// Close material library
				fclose(hMaterialLib);
			}
		}
			
		// Clear string two avoid counting something twice
		memset(szString, '\0', sizeof(szString));
	}
}

bool _stdcall COBJModel::ReadNextString(char szString[], FILE *hStream)
{
////////////////////////////////////////////////////////////////////////
// Read the next string that isn't a comment
////////////////////////////////////////////////////////////////////////

	for (;;)
	{
		// Every caller supplies the fixed MAX_STR_SIZE buffer (1024 bytes).
		if (fscanf(hStream, "%1023s", szString) != 1)
		{
			szString[0] = '\0';
			return false;
		}
		// Is rest of the line a comment ?
		if (strncmp(szString, COMMENT_ID, sizeof(COMMENT_ID)) != 0)
		{
			return true;
		}
		// Skip the whole comment even if it exceeds the token buffer.
		int character = 0;
		while ((character = fgetc(hStream)) != '\n' && character != EOF) {}
	}
}


void _stdcall COBJModel::MakePath(char szFileAndPath[])
{
////////////////////////////////////////////////////////////////////////
// Rips the filenames out of a path and adds a slash (if needed)
////////////////////////////////////////////////////////////////////////
	
// Get string length
	int iNumChars = (int)strlen(szFileAndPath);

	// Loop from the last to the first char
	for (int iCurChar=iNumChars-1; iCurChar>=0; iCurChar--)
	{
		// If the current char is a slash / backslash
		if (szFileAndPath[iCurChar] == char('\\') ||
			szFileAndPath[iCurChar] == char('/'))
		{
			// Terminate the the string behind the slash / backslash
			szFileAndPath[iCurChar + 1] = char('\0');
			return;
		}
	}

// No slash there, set string length to zero
	szFileAndPath[0] = char('\0');
}

void _stdcall COBJModel::GetTokenParameter(char szString[], 
								  const unsigned int iStrSize, FILE *hFile)
{
	if (!fgets(szString, iStrSize, hFile)) {
		szString[0] = '\0';
		return;
	}

	size_t inicio = strspn(szString, " \t");
	memmove(
		szString,
		szString + inicio,
		strlen(szString + inicio) + 1
	);

	szString[strcspn(szString, "\r\n")] = '\0';
}

static int CompareFaceByMaterial(const void *Arg1, const void *Arg2)
{
////////////////////////////////////////////////////////////////////////
// Callback function for comparing two faces with qsort
////////////////////////////////////////////////////////////////////////

// Cast the void pointers to faces
	Face *Face1 = (Face*) Arg1;
	Face *Face2 = (Face*) Arg2;

// Greater
	if (Face1->iMaterialIndex > Face2->iMaterialIndex)
		return 1;

// Less
	if (Face1->iMaterialIndex < Face2->iMaterialIndex)
		return -1;

// Equal
	return 0;
}


int COBJModel::LoadTexture2(const char szFileName[_MAX_PATH])
{
////////////////////////////////////////////////////////////////////////
// Load a texture and return its ID
////////////////////////////////////////////////////////////////////////

	FILE *file=NULL;
	int err;
	unsigned int iTexture = 0;

	
// Open the image file for reading
// file=fopen(filename,"r");					// Funci� Visual Studio 6.0
   err=fopen_s(&file,szFileName,"r");			// Funci� Visual 2005

// If the file is empty (or non existent) print an error and return false
// if (file == NULL)
   if (err!=0)
	 {	//	printf("Could not open file '%s'.\n",filename) ;
	    return false ;
	}

// Close the image file
 fclose(file);

// SOIL_load_OGL_texture: Funci� que llegeix la imatge del fitxer filename
//				si �s compatible amb els formats SOIL (BMP,JPG,GIF,TIF,TGA,etc.)
//				i defineix la imatge com a textura OpenGL retornant l'identificador 
//				de textura OpenGL.
 iTexture = SOIL_load_OGL_texture
	 (	 szFileName,
		SOIL_LOAD_AUTO,
		SOIL_CREATE_NEW_ID,
		SOIL_FLAG_MIPMAPS | SOIL_FLAG_DDS_LOAD_DIRECT | SOIL_FLAG_INVERT_Y
	 );


// If execution arrives here it means that all went well. Return true
 
// Make the texture the current one
	glBindTexture(GL_TEXTURE_2D, iTexture);

// Build mip-maps
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	return iTexture;
}

void _stdcall COBJModel::DrawModel(int prim_Id)
{
	//glCallList(m_iDisplayList);
	deleteVAOList(prim_Id);
}

  
// Create Object Instance-Access Function
OBJLOADER_CLASS_DECL COBJModel* _stdcall InitObject()
{
  return new COBJModel; //Alloc TMyObject instance
}

// Release Object Instance-Access Function
OBJLOADER_CLASS_DECL void _stdcall UnInitObject(COBJModel * obj) 
{
  delete obj; //Release TMyObject instance
}

//void _stdcall COBJModel::EliminaLlista(unsigned int iDisplayList)
void _stdcall COBJModel::EliminaLlista(int prim_Id)
{
	//glDeleteLists(iDisplayList, 1);
	deleteVAOList(prim_Id);
}

// ------------------------ GESTI� VAOLIST
void _stdcall COBJModel::initVAOList_OBJ()
{
	int i;
	for (i = 0; i < MAX_SIZE_VAOLIST; i++)
	{	VAOList_OBJ[i].vaoId = 0;
		VAOList_OBJ[i].vboId = 0;
		VAOList_OBJ[i].eboId = 0;
		VAOList_OBJ[i].nVertexs = 0;
		VAOList_OBJ[i].nIndices = 0;
	}

}

void _stdcall COBJModel::Set_VAOList_OBJ(GLint k, CVAO auxVAO)
{
	VAOList_OBJ[k].vaoId = auxVAO.vaoId;
	VAOList_OBJ[k].vboId = auxVAO.vboId;
	VAOList_OBJ[k].eboId = auxVAO.eboId;
	VAOList_OBJ[k].nVertexs = auxVAO.nVertexs;
	VAOList_OBJ[k].nIndices = auxVAO.nIndices;
}


void _stdcall COBJModel::netejaVAOList_OBJ()
{
	GLint i;
	for (i = 0; i < MAX_SIZE_VAOLIST; i++)
	{
		if (VAOList_OBJ[i].vaoId != 0) deleteVAOList_OBJ(i);
	}
}


void _stdcall COBJModel::netejaTextures_OBJ()
{
	int i;

	for (i = 0; i < static_cast<int>(materialCount); i++)
	{
		if (vMaterials[i].iTextureID)
		{
			glDeleteTextures(1, &vMaterials[i].iTextureID);
			vMaterials[i].iTextureID = 0;
		}
	}
	materialCount = 0;
	numMaterials = 0;
}

void _stdcall COBJModel::deleteVAOList_OBJ(GLint k)
{
	//GLboolean err;

	if (VAOList_OBJ[k].vaoId != 0)
	{	glBindVertexArray(VAOList_OBJ[k].vaoId); //glBindVertexArray(vaoId);
		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);
		glDisableVertexAttribArray(3);

		// It is good idea to release VBOs with ID 0 after use.
		// Once bound with 0, all pointers in gl*Pointer() behave as real
		// pointer, so, normal vertex array operations are re-activated
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		//err = glIsBuffer(VAOList_OBJ[k].vboId);
		glDeleteBuffers(1, &VAOList_OBJ[k].vboId);// glDeleteBuffers(1, &vboId);
		//err = glIsBuffer(VAOList_OBJ[k].vboId);

		// Delete EBO
		if (VAOList_OBJ[k].eboId != 0) glDeleteBuffers(1, &VAOList_OBJ[k].eboId);// glDeleteBuffers(1, &eboId);

		// Unbind and delete VAO
		glBindVertexArray(0);
		glDeleteVertexArrays(1, &VAOList_OBJ[k].vaoId);	// glDeleteVertexArrays(1, &vaoId);

		VAOList_OBJ[k].vaoId = 0;
		VAOList_OBJ[k].vboId = 0;
		VAOList_OBJ[k].eboId = 0;
		VAOList_OBJ[k].nVertexs = 0;
		VAOList_OBJ[k].nIndices = 0;
	}
}


//--------------- DRAWS (DIBUIX DE VAO's
void _stdcall COBJModel::draw_TriVAO_Object_OBJ(GLint k)
{
	// Recuperar identificadors VAO i nvertexs dels vector VAOList pr a dibuixar VAO
	if (VAOList_OBJ[k].vaoId != 0) {
		glBindVertexArray(VAOList_OBJ[k].vaoId);
		//glBindBuffer(GL_ARRAY_BUFFER, vboId);
		glDrawArrays(GL_TRIANGLES, 0, VAOList_OBJ[k].nVertexs);
		glBindVertexArray(0);
	}
}


void _stdcall COBJModel::draw_TriVAO_OBJ(GLuint sh_programID)
{
	int i;

	for (i = 0; i < numMaterials; i++)
	{
		const int materialIndex = vector_Materials[i];
		if (materialIndex >= 0 && static_cast<unsigned int>(materialIndex) < materialCount)
			UseMaterial_ShaderID(sh_programID, vMaterials[materialIndex]);
		else
			glUniform1i(glGetUniformLocation(sh_programID, "textur"), GL_FALSE);
		draw_TriVAO_Object_OBJ(i);
	}
}
