#include <pluginshare.h>
#include <mdx_man.h>

char const *g_pPluginName = "dukemdx";
char const *g_pPluginDesc = "Duke Nukem Extended Model format handler.";
int g_fmtHandle = -1;

bool Model_MDX_Check(BYTE *fileBuffer, int bufferLen, noeRAPI_t *rapi)
{
	if (bufferLen < sizeof(ascfheader_t))
		return false;

	auto header = (ascfheader_t *)fileBuffer;

	if (header->marker != ASCFMARKER || header->ascfVersion != ASCFVERSION)
		return false;

	if (header->typeMarker != ASCFDNXMMARKER || header->typeVersion != ASCFDNXMVERSION)
		return false;

	if (header->fileSize > bufferLen)
		return false;

	return true;
}

ascfentry_t *MDX_FindEntry(ascfheader_t *header, uint32_t label)
{
	ascfentry_t *start = (ascfentry_t *)((BYTE *)header + header->dirOfs);

	for (size_t i = 0; i < header->dirEntries; i++)
	{
		if (start[i].chunkLabel == label)
		{
			return &start[i];
		}
	}

	return nullptr;
}

template<class T>
T *MDX_GetEntryData(ascfheader_t *header, uint32_t label)
{
	return (T *)((BYTE *)header + MDX_FindEntry(header, label)->chunkOfs);
}

void MDX_DecodeUV(mdxtvert_t *tv, float *uv, mdxskin_t *skin)
{
	uv[0] = (float)tv->s / (skin ? skin->skinWidth : 255);
	uv[1] = (float)tv->t / (skin ? skin->skinHeight : 255);
}

void MDX_DecodeNormal(mdxvert_t *v, float *nrm)
{
	nrm[0] = (float)(v->normal[0] & ~128) / 127.f;
	nrm[1] = (float)(v->normal[1] & ~128) / 127.f;
	nrm[2] = (float)(v->normal[2] & ~128) / 127.f;

	if (v->normal[0] & 128)
		nrm[0] *= -1;

	if (v->normal[1] & 128)
		nrm[1] *= -1;

	if (v->normal[2] & 128)
		nrm[2] *= -1;
}

void MDX_DecodeVertex(mdxframeinfo_t *frameinfo, mdxvert_t *v, float *pos)
{
	float *scale = frameinfo->scales[v->groupNum & 0x0F];
	float *translate = frameinfo->translates[v->groupNum & 0x0F];
	pos[0] = v->v[0] * scale[0] + translate[0];
	pos[1] = v->v[1] * scale[1] + translate[1];
	pos[2] = v->v[2] * scale[2] + translate[2];
}

noesisModel_t *Model_MDX_Load(BYTE *fileBuffer, int bufferLen, int &numMdl, noeRAPI_t *rapi)
{
	auto context = rapi->rpgCreateContext();
	auto header = (ascfheader_t *)fileBuffer;
	auto entries = (ascfentry_t *)(fileBuffer + header->dirOfs);
	auto rfrm = MDX_GetEntryData<mdxrfrmchunk_t>(header, MAKEMARKER("RFRM"));
	auto tris = MDX_GetEntryData<mdxtrischunk_t>(header, MAKEMARKER("TRIS"));
	auto skin = MDX_GetEntryData<mdxskinchunk_t>(header, MAKEMARKER("SKIN"));
	auto frameCount = 0u;
	auto sequenceCount = 0u;

	// add all frames
	for (auto i = 0; i < header->dirEntries; i++)
	{
		if (entries[i].chunkLabel == MAKEMARKER("FSEQ") && entries[i].chunkVersion == 3)
			sequenceCount++;

		if (entries[i].chunkLabel != MAKEMARKER("FRMD") || entries[i].chunkVersion != 1)
			continue;

		frameCount++;
		auto frmd = (mdxfrmdchunk_t *)(fileBuffer + entries[i].chunkOfs);
		auto xyz = (float *)rapi->Noesis_PooledAlloc(sizeof(float) * 3 * rfrm->numVerts);
		auto data = (uint16_t *)frmd->info;

		for (auto cmd = ((*data) & 0xF000) >> 12; cmd; cmd = ((*data) & 0xF000) >> 12)
		{
			switch (cmd)
			{
			case 1:
				{
					auto m = (*data) & 0x0FFF; data++;
					auto v = (mdxvert_t *)data; data += sizeof(mdxvert_t) / 2;
					MDX_DecodeVertex(&frmd->frameInfo, v, &xyz[3 * m]);
					break;
				}
			case 2:
				{
					auto m = (*data) & 0x0FFF; data++;
					auto c = *data + m; data++;

					for (int i = m; i < c; i++)
					{
						auto v = (mdxvert_t *)data; data += sizeof(mdxvert_t) / 2;
						MDX_DecodeVertex(&frmd->frameInfo, v, &xyz[3 * i]);
					}
					break;
				}
			}
		}

		rapi->rpgFeedMorphName(entries[i].chunkInstance);
		rapi->rpgFeedMorphTargetPositions(xyz, RPGEODATA_FLOAT, sizeof(float) * 3);
		rapi->rpgCommitMorphFrame(rfrm->numVerts);
	}

	rapi->rpgCommitMorphFrameSet();
	
	auto verts = (mdxvert_t *)rfrm->info;
	auto basetris = (mdxtvert_t *)(verts + rfrm->numVerts);
	auto skinidxs = (uint8_t *)(basetris + (rfrm->numTris * 3));

	for (int32_t i = 0; i < rfrm->numTris; i++)
	{
		float pos[3];
		float nrm[3];
		float uv[2];
		auto tri = &tris->tris[i];
		auto v0 = &verts[tri->vertIndex[0]];
		auto v1 = &verts[tri->vertIndex[1]];
		auto v2 = &verts[tri->vertIndex[2]];

		mdxskin_t *tskn = nullptr;

		if (skin && skinidxs[i] < skin->numSkins)
			tskn = &skin->skins[skinidxs[i]];

		rapi->rpgSetMaterial(skin->skins[skinidxs[i]].skinFile);
		rapi->rpgBegin(RPGEO_TRIANGLE);

		MDX_DecodeNormal(v0, nrm);
		MDX_DecodeUV(&basetris[3 * i + 0], uv, tskn);
		MDX_DecodeVertex(&rfrm->frameInfo, v0, pos);
		rapi->rpgVertUV2f(uv, 0);
		rapi->rpgVertNormal3f(nrm);
		rapi->rpgVertMorphIndex(tri->vertIndex[0]);
		rapi->rpgVertex3f(pos);

		MDX_DecodeNormal(v2, nrm);
		MDX_DecodeUV(&basetris[3 * i + 2], uv, tskn);
		MDX_DecodeVertex(&rfrm->frameInfo, v2, pos);
		rapi->rpgVertUV2f(uv, 0);
		rapi->rpgVertNormal3f(nrm);
		rapi->rpgVertMorphIndex(tri->vertIndex[2]);
		rapi->rpgVertex3f(pos);

		MDX_DecodeNormal(v1, nrm);
		MDX_DecodeUV(&basetris[3 * i + 1], uv, tskn);
		MDX_DecodeVertex(&rfrm->frameInfo, v1, pos);
		rapi->rpgVertUV2f(uv, 0);
		rapi->rpgVertNormal3f(nrm);
		rapi->rpgVertMorphIndex(tri->vertIndex[1]);
		rapi->rpgVertex3f(pos);

		rapi->rpgEnd();
	}

	rapi->rpgOptimize();
	auto mdl = rapi->rpgConstructModel();

	if (mdl)
	{
		numMdl = 1;
		rapi->SetPreviewAnimSpeed(10.f);
		static float mdlAngOfs[3] = {0.0f, 270.0f, 90.0f};
		rapi->SetPreviewAngOfs(mdlAngOfs);
	}

	rapi->rpgDestroyContext(context);
	return mdl;
}

bool NPAPI_InitLocal(void)
{
	g_fmtHandle = g_nfn->NPAPI_Register("Duke Nukem Extended Model", ".mdx");

	if (g_fmtHandle < 0)
		return false;

	g_nfn->NPAPI_SetTypeHandler_TypeCheck(g_fmtHandle, Model_MDX_Check);
	g_nfn->NPAPI_SetTypeHandler_LoadModel(g_fmtHandle, Model_MDX_Load);
	return true;
}

void NPAPI_ShutdownLocal(void)
{
}
