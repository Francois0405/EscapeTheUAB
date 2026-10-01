//******** PRACTICA VISUALITZACIÓ GRÀFICA INTERACTIVA (Escola Enginyeria - UAB)
//******** Entorn bàsic VS2026 MONOFINESTRA amb OpenGL 4.6, interfície GLFW 3.4, ImGui i llibreries GLM
//******** Enric Martí (Setembre 2026)
// objLoader.cpp: Implements the class COBJModel.
//
//	  Versió 2.0:	- Adaptació funcions a crear un VAO per a cada material del fitxer
//////////////////////////////////////////////////////////////////////////////////////
//           Wavefront OBJ Loader (C) 2000 Tim C. Schröder
// -------------------------------------------------------------------
//    tcs_web@gmx.de / tcs_web@hotmail.com / tcs@thereisnofate.net
//                 http://glvelocity.demonews.com
//////////////////////////////////////////////////////////////////////

#include "framework.h"
#include "objLoader.h"

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
	strcpy(szBasePath, szFileName);
	MakePath(szBasePath);

////////////////////////////////////////////////////////////////////////
// Open the OBJ file
////////////////////////////////////////////////////////////////////////
	errno = 0;
	errno = fopen_s(&hFile, szFileName, "r"); // Funció Visual 2005 i 2010
	
// Success ?
	if (errno != 0)
		return errno; //FALSE;

////////////////////////////////////////////////////////////////////////
// Allocate space for structures that hold the model data
////////////////////////////////////////////////////////////////////////
 
// Which data types are stored in the file ? How many of each type ?
	GetFileInfo(hFile, &OBJInfo, szBasePath);

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
		pMaterials = new Material[OBJInfo.iMaterialCount];

	// Init structure that holds the current array index
	memset(&CurrentIndex, 0, sizeof(OBJFileInfo));

////////////////////////////////////////////////////////////////////////
// Read the file contents
////////////////////////////////////////////////////////////////////////

// Start reading the file from the start
	rewind(hFile);
	
// Quit reading when end of file has been reached
	while (!feof(hFile))
	{
		// Get next string
		ReadNextString(szString, hFile);

		// Next three elements are floats of a vertex
		if (!strncmp(szString, VERTEX_ID, sizeof(VERTEX_ID)))
		{
			// Read three floats out of the file
			nScanReturn = fscanf_s(hFile, "%f %f %f",
				&pVertices[CurrentIndex.iVertexCount].fX,
				&pVertices[CurrentIndex.iVertexCount].fY,
				&pVertices[CurrentIndex.iVertexCount].fZ);
			// Next vertex
			CurrentIndex.iVertexCount++;
		}

		// Next two elements are floats of a texture coordinate
		if (!strncmp(szString, TEXCOORD_ID, sizeof(TEXCOORD_ID)))
		{
			// Read two floats out of the file
			nScanReturn = fscanf_s(hFile, "%f %f",
				&pTexCoords[CurrentIndex.iTexCoordCount].fX,
				&pTexCoords[CurrentIndex.iTexCoordCount].fY);
			// Next texture coordinate
			CurrentIndex.iTexCoordCount++;
		}

		// Next three elements are floats of a vertex normal
		if (!strncmp(szString, NORMAL_ID, sizeof(NORMAL_ID)))
		{
			// Read three floats out of the file
			nScanReturn = fscanf_s(hFile, "%f %f %f",
				&pNormals[CurrentIndex.iNormalCount].fX,
				&pNormals[CurrentIndex.iNormalCount].fY,
				&pNormals[CurrentIndex.iNormalCount].fZ);
			// Next normal
			CurrentIndex.iNormalCount++;
		}

		// Rest of the line contains face information
		if (!strncmp(szString, FACE_ID, sizeof(FACE_ID)))
		{	// Read the rest of the line (the complete face)
			GetTokenParameter(szString, sizeof(szString) ,hFile);
			// Convert string into a face structure
			ParseFaceString(szString, &pFaces[CurrentIndex.iFaceCount],
				pVertices, pNormals, pTexCoords, iCurMaterial);
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
				strcpy_s(szLibraryFile, szBasePath);
				strcat_s(szLibraryFile, szString);
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
		for (int i = 0; i < numMaterials; i++) vMaterials[i] = pMaterials[i];
	}

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

void _stdcall COBJModel::ParseFaceString(char szFaceString[], Face *FaceOut, 
								const Vector3D *pVertices, 
								const Vector3D *pNormals,
								const Vector2D *pTexCoords,
								const unsigned int iMaterialIndex)
{
////////////////////////////////////////////////////////////////////////
// Convert face string from the OBJ file into a face structure
////////////////////////////////////////////////////////////////////////
	
	int i;
	int iVertex = 0, iTextureCoord = 0, iNormal = 0;

	// Pointer to the face string. Will be incremented later to
	// advance to the next triplet in the string.
	char *pFaceString = szFaceString;

	// Save the string positions of all triplets
	int iTripletPos[MAX_VERTICES];
	int iCurTriplet = 0;

	// Init the face structure
	memset(FaceOut, 0, sizeof(Face));

////////////////////////////////////////////////////////////////////////
// Get number of vertices in the face
////////////////////////////////////////////////////////////////////////

// Loop trough the whole string
	for (i=0; i<(int) strlen(szFaceString); i++)
	{	// Each triplet is separated by spaces
		if (szFaceString[i] == ' ')
		{	// One more vertex
			FaceOut->iNumVertices++;
			// Save position of triplet
			iTripletPos[iCurTriplet] = i;
			// Next triplet
			iCurTriplet++;
		}
	}

// Face has more vertices than spaces that separate them
	FaceOut->iNumVertices++;

////////////////////////////////////////////////////////////////////////
// Allocate space for structures that hold the face data
////////////////////////////////////////////////////////////////////////

// Vertices
	FaceOut->pVertices		= new Vector3D[FaceOut->iNumVertices];

// Allocate space for normals and texture coordinates only if present
	if (pNormals)
		FaceOut->pNormals	= new Vector3D[FaceOut->iNumVertices];
	if (pTexCoords)
		FaceOut->pTexCoords = new Vector2D[FaceOut->iNumVertices];
	
////////////////////////////////////////////////////////////////////////
// Copy vertex, normal, material and texture data into the structure
////////////////////////////////////////////////////////////////////////

// Set material
	FaceOut->iMaterialIndex = iMaterialIndex;

// Process per-vertex data
	for (i=0; i<(int) FaceOut->iNumVertices; i++)
	{
		// Read one triplet from the face string

		// Are vertices, normals and texture coordinates present ?
		if (pNormals && pTexCoords)
			// Yes
			sscanf_s(pFaceString, "%i/%i/%i", 
				&iVertex, &iTextureCoord, &iNormal);
		else if (pNormals && !pTexCoords)
			// Vertices and normals but no texture coordinates
			sscanf_s(pFaceString, "%i//%i", &iVertex, &iNormal);
		else if (pTexCoords && !pNormals)
			// Vertices and texture coordinates but no normals
			sscanf_s(pFaceString, "%i/%i", &iVertex, &iTextureCoord);
		else
			// Only vertices
			sscanf_s(pFaceString, "%i", &iVertex);

		// Copy vertex into the face. Also check for normals and texture 
		// coordinates and copy them if present.
		memcpy(&FaceOut->pVertices[i], &pVertices[iVertex - 1], 
			sizeof(Vector3D));
		if (pTexCoords)
			memcpy(&FaceOut->pTexCoords[i], 
			&pTexCoords[iTextureCoord - 1], sizeof(Vector2D));
		if (pNormals)
			memcpy(&FaceOut->pNormals[i], 
			&pNormals[iNormal - 1], sizeof(Vector3D));

		// Set string pointer to the next triplet
		pFaceString = &szFaceString[iTripletPos[i]];
	}
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
	int errno;

////////////////////////////////////////////////////////////////////////
// Open library file
////////////////////////////////////////////////////////////////////////
	errno = fopen_s(&hFileT,szFileName, "r");
	// Success ?
	if (errno!=0) //(!hFile)
		return FALSE;

////////////////////////////////////////////////////////////////////////
// Read all material definitions
////////////////////////////////////////////////////////////////////////

// Quit reading when end of file has been reached
	while (!feof(hFileT))
	{
		// Get next string
		ReadNextString(szString, hFileT);

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
			strcpy_s(szTextureFile, szBasePath);
			strcat_s(szTextureFile, szString);
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

	numMaterials = *iCurMaterialIndex;
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

	std::vector <double> vertices, colors, normals, textures;		// Definició vectors dinàmics per a vertexs i colors 
	vertices.resize(0);		colors.resize(0);	normals.resize(0);		textures.resize(0);// Reinicialitzar vectors
	
// Obtenir color actual definit en OpenGL amb glColor();
	GLfloat cColor[4];
	glGetFloatv(GL_CURRENT_COLOR, cColor);

// Generate & save display list index
	
// Render model into the display list
	//glNewList(m_iDisplayList, GL_COMPILE);

		// Save texture bit to recover from the various texture state changes
		glPushAttrib(GL_TEXTURE_BIT);

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
		glPopAttrib();
	//glEndList();

// ----------------------- VAO
// Creació d'un VAO i un VBO i càrrega de la geometria. Guardar identificador VAO identificador VBO a struct CVAO.
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

	std::vector <double> vertices, colors, normals, textures;		// Definició vectors dinàmics per a vertexs i colors 
	vertices.resize(0);		colors.resize(0);	normals.resize(0);		textures.resize(0);// Reinicialitzar vectors

	std::vector <int>::size_type nv = vertices.size();	// Tamany del vector vertices en elements.

	netejaVAOList_OBJ();

// Obtenir color actual definit en OpenGL amb glColor();
	GLfloat cColor[4];
	glGetFloatv(GL_CURRENT_COLOR, cColor);

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
						// Creació d'un VAO i un VBO i càrrega de la geometria. Guardar identificador VAO identificador VBO a struct CVAO.
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
	{	// Creació del darrer VAO i un VBO i càrrega de la geometria. Guardar identificador VAO identificador VBO a struct CVAO.
		objVAO = load_TRIANGLES_VAO(vertices, normals, colors, textures);
		Set_VAOList_OBJ(index_VAO, objVAO);
	}

// Numero de Materials del fitxer OBJ
	numMaterials = index_VAO + 1;
}


void _stdcall COBJModel::UseMaterial(const Material *pMaterial)
{
////////////////////////////////////////////////////////////////////////
// Make a given material the current one
////////////////////////////////////////////////////////////////////////
	float color[4] = { 1.0F, 0.0f, 0.0f, 1.0f};

	//glColor3f(1.0,1.0,1.0);
// Look for the presence of a texture and activate texturing if succeed
	if (pMaterial!=NULL)
	{ if (pMaterial->iTextureID)
		{	glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, pMaterial->iTextureID);
		}
	else {	glDisable(GL_TEXTURE_2D);
			//glMaterialfv(GL_FRONT, GL_EMISSION, pMaterial->fEmmissive);
			
			glColorMaterial(GL_FRONT, GL_AMBIENT);
			color[0] = pMaterial->fAmbient[0];	color[1] = pMaterial->fAmbient[1];	color[2] = pMaterial->fAmbient[2];
			//glMaterialfv(GL_FRONT, GL_AMBIENT, pMaterial->fAmbient);
			glMaterialfv(GL_FRONT, GL_AMBIENT, color);
			glColor4fv(color);

			glColorMaterial(GL_FRONT, GL_DIFFUSE);
			color[0] = pMaterial->fDiffuse[0];	color[1] = pMaterial->fDiffuse[1];	color[2] = pMaterial->fDiffuse[2];
			//glMaterialfv(GL_FRONT, GL_DIFFUSE, pMaterial->fDiffuse);
			glMaterialfv(GL_FRONT, GL_DIFFUSE, color);
			glColor4fv(color);
			
			glColorMaterial(GL_FRONT, GL_SPECULAR);
			color[0] = pMaterial->fSpecular[0];	color[1] = pMaterial->fSpecular[1];	color[2] = pMaterial->fSpecular[2];
			//glMaterialfv(GL_FRONT, GL_SPECULAR, pMaterial->fSpecular);
			glMaterialfv(GL_FRONT, GL_SPECULAR, color);
			glColor4fv(color);

			glMaterialf(GL_FRONT, GL_SHININESS, pMaterial->fShininess);

			glEnable(GL_COLOR_MATERIAL);
		}
	}
	else {	glDisable(GL_TEXTURE_2D);
		}
}


void _stdcall COBJModel::UseMaterial_ShaderID(GLuint sh_programID, Material pMaterial)
{
////////////////////////////////////////////////////////////////////////
// Make a given material the current one
////////////////////////////////////////////////////////////////////////
	float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	//glColor3f(1.0,1.0,1.0);
// Look for the presence of a texture and activate texturing if succeed
	if (pMaterial.szName != "")
	{
		//glUniform4i(glGetUniformLocation(sh_programID, "sw_intensity"), false, true, true, true);
		//glUniform1i(glGetUniformLocation(sh_programID, "sw_material"), true);

		if (pMaterial.iTextureID)
		{
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, pMaterial.iTextureID);
			glUniform1i(glGetUniformLocation(sh_programID, "texture0"), GLint(0));
			//glUniform1i(glGetUniformLocation(sh_programID, "texture0"), pMaterial.iTextureID);

			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		}
		else {
			glDisable(GL_TEXTURE_2D);
			//glMaterialfv(GL_FRONT, GL_EMISSION, pMaterial->fEmmissive);

			//glColorMaterial(GL_FRONT, GL_AMBIENT);
			color[0] = pMaterial.fAmbient[0];	color[1] = pMaterial.fAmbient[1];	color[2] = pMaterial.fAmbient[2];
			//glMaterialfv(GL_FRONT, GL_AMBIENT, pMaterial->fAmbient);
			//glMaterialfv(GL_FRONT, GL_AMBIENT, color);
			//glColor4fv(color);
			glUniform4f(glGetUniformLocation(sh_programID, "material.ambient"), color[0], color[1], color[2], color[3]);

			//glColorMaterial(GL_FRONT, GL_DIFFUSE);
			color[0] = pMaterial.fDiffuse[0];	color[1] = pMaterial.fDiffuse[1];	color[2] = pMaterial.fDiffuse[2];
			//glMaterialfv(GL_FRONT, GL_DIFFUSE, pMaterial->fDiffuse);
			//glMaterialfv(GL_FRONT, GL_DIFFUSE, color);
			//glColor4fv(color);
			glUniform4f(glGetUniformLocation(sh_programID, "material.diffuse"), color[0], color[1], color[2], color[3]);

			//glColorMaterial(GL_FRONT, GL_SPECULAR);
			color[0] = pMaterial.fSpecular[0];	color[1] = pMaterial.fSpecular[1];	color[2] = pMaterial.fSpecular[2];
			//glMaterialfv(GL_FRONT, GL_SPECULAR, pMaterial->fSpecular);
			//glMaterialfv(GL_FRONT, GL_SPECULAR, color);
			//glColor4fv(color);
			glUniform4f(glGetUniformLocation(sh_programID, "material.specular"), color[0], color[1], color[2], color[3]);

			//glMaterialf(GL_FRONT, GL_SHININESS, pMaterial->fShininess);
			glUniform1f(glGetUniformLocation(sh_programID, "material.shininess"), pMaterial.fShininess);

			glEnable(GL_COLOR_MATERIAL);
			}
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
	int errno;

// Init structure
	memset(Info, 0, sizeof(OBJFileInfo));

// Rollback the stream
	rewind(hStream);

// Quit reading when end of file has been reached
	while (!feof(hStream))
	{
		// Get next string
		ReadNextString(szString, hStream);

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
			strcpy_s(szBasePath, szConstBasePath);
			// Append material library filename to the model's base path
			strcat_s(szBasePath, szString);

			// Open the library file
//			FILE *hMaterialLib = fopen(szBasePath, "r");
			errno = fopen_s(&hMaterialLib,szBasePath, "r");
			// Success ?
			if (!errno) //(hMaterialLib)
			{
				// Quit reading when end of file has been reached
				while (!feof(hMaterialLib))
				{
					// Read next string
					fscanf(hMaterialLib, "%s" ,szString);
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

void _stdcall COBJModel::ReadNextString(char szString[], FILE *hStream)
{
////////////////////////////////////////////////////////////////////////
// Read the next string that isn't a comment
////////////////////////////////////////////////////////////////////////

	bool bSkipLine = FALSE;	// Skip the current line ?
	int nScanReturn = 0;	// Return value of fscanf

	// Skip all strings that contain comments
	do
	{
		// Read new string
		nScanReturn = fscanf(hStream, "%s", szString);
		// Is rest of the line a comment ?
		if (!strncmp(szString, COMMENT_ID, sizeof(COMMENT_ID)))
		{
			// Skip the rest of the line
			fgets(szString, sizeof(szString), hStream);
			bSkipLine = TRUE;
		}
		else bSkipLine = FALSE;
	} while (bSkipLine == TRUE);
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
////////////////////////////////////////////////////////////////////////
// Read the parameter of a token, remove space and newline character
////////////////////////////////////////////////////////////////////////

// Read the parameter after the token
	fgets(szString, iStrSize, hFile);

// Remove space before the token			
	strcpy(szString, &szString[1]);

// Remove newline character after the token
	szString[strlen(szString) - 1] = char('\0');
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
	int errno;
	unsigned int iTexture = 0;

	
// Open the image file for reading
// file=fopen(filename,"r");					// Funció Visual Studio 6.0
   errno=fopen_s(&file,szFileName,"r");			// Funció Visual 2005

// If the file is empty (or non existent) print an error and return false
// if (file == NULL)
   if (errno!=0)
	 {	//	printf("Could not open file '%s'.\n",filename) ;
	    return false ;
	}

// Close the image file
 fclose(file);

// SOIL_load_OGL_texture: Funció que llegeix la imatge del fitxer filename
//				si és compatible amb els formats SOIL (BMP,JPG,GIF,TIF,TGA,etc.)
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

// ------------------------ GESTIÓ VAOLIST
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
	GLboolean err;

	for (i = 0; i <= numMaterials; i++)
	{	if (vMaterials[i].iTextureID)
		{	err = glIsTexture(vMaterials[i].iTextureID);
			glDeleteTextures(1, &vMaterials[i].iTextureID);
			err = glIsTexture(vMaterials[i].iTextureID);
			// Entorn VGI: Eliminar valors de textura
			vMaterials[i].iTextureID = 0;
			
		}
	}
	numMaterials = 0; // Inicialitzar nombre de materials a 0
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

	for (i = 0; i <= numMaterials; i++)
	{	UseMaterial_ShaderID(sh_programID, vMaterials[i]);	// Activació Material i-èssim
		
		draw_TriVAO_Object_OBJ(i);							// Dibuix objecte i-èssim
	}
}