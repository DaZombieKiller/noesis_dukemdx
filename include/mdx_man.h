#ifndef __MDX_MAN_H__
#define __MDX_MAN_H__
#pragma pack(push, 1)
//****************************************************************************
//**
//**    MDX_MAN.H
//**    Header - MDX Management
//**
//****************************************************************************
//----------------------------------------------------------------------------
//    Headers
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//    Definitions
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//    Class Prototypes
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//    Required External Class References
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//    Structures
//----------------------------------------------------------------------------
/*
========================================================================

  ASCF chunked format (used for MDX)

========================================================================
*/
#define MAKEMARKER(name) ((name)[0] | ((name)[1] << 8) | ((name)[2] << 16) | ((name)[3] << 24))

// ASCF Standard File Header
#define ASCFMARKER MAKEMARKER("ASCF")
#define ASCFVERSION 3

typedef struct
{
	uint32_t marker; // "ASCF" for Apogee Software Chunk Format
	uint32_t typeMarker; // File-type specific marker
    int16_t ascfVersion; // ASCFVERSION
	int16_t typeVersion; // File-type specific version
	uint32_t fileSize; // total size of file
	uint32_t dirOfs; // chunk directory offset
	uint32_t dirEntries; // chunk directory entry count (directory size = dirEntries*sizeof(ascfentry_t))
	uint32_t user1; // additional user data space
    uint32_t user2; // additional user data space
} ascfheader_t;

// ASCF Standard Chunk Entry
typedef struct
{
	uint32_t chunkLabel; // four-character label, see list below
	uint32_t chunkOfs; // chunk data offset
	uint32_t chunkLen; // chunk data length in bytes
    uint8_t chunkVersion; // chunk-specific version number
    char reserved[3]; // must be zero
    char chunkInstance[32]; // instance name of this chunk, for use by other chunks
} ascfentry_t;


// internal representation
typedef struct ascfentrylink_s ascfentrylink_t;
struct ascfentrylink_s
{
	ascfentry_t entry;
	void *data;
	ascfentrylink_t *next;
};

typedef struct
{
	ascfheader_t header;
	ascfentrylink_t *dir;
} mdxChunkFile_t;


/*
========================================================================

  .MDX extended model file format

========================================================================
*/

#define ASCFDNXMMARKER MAKEMARKER("DNXM")
#define ASCFDNXMVERSION 5

/*
Header Data:
	typeMarker: "DNXM" for Duke Nukem Extended Model
    typeVersion: ASCFDNXMVERSION

Chunks:
    SKIN        Skins
	TRIS		Triangles
	MPNT		Mount Points
    RFRM        Reference Frame
    FRMD        Frame Deformation
    FSEQ        Frame Sequence

Auxiliary Chunks:
	CBLP		Cannibal Project Info - Ignored outside Cannibal
*/

// _____SKIN Chunk (only one per file)

typedef struct
{
	int32_t skinWidth; // width of skin data
	int32_t skinHeight; // height of skin data
	int32_t skinBitDepth; // bit depth of skin data
    char skinFile[64]; // skin filename relative to model location, no extention
} mdxskin_t;

typedef struct
{
	int32_t numSkins;
	mdxskin_t skins[1]; // variable sized
} mdxskinchunk_t;


// _____TRIS Chunk (only one per file)

typedef struct
{
	int16_t vertIndex[3];
    uint16_t edgeIndex[3]; // tris for linked tris at v0v1,v1v2,v2v0... used for tristrips, some effects, etc.
	                    // low 14 bits is index, high 2 bits is edge number on the other tri, 0-2
    int16_t flags;
    uint8_t aux1;
	uint8_t aux2;
} mdxtri_t;

typedef struct
{
	int32_t numTris; // number of triangles
    mdxtri_t tris[1]; // variable sized
} mdxtrischunk_t;


// _____MPNT Chunk (only one per file)

typedef struct
{
	int32_t triIndex; // triangle index as basis of mount point, vertex indices are v0-v2.  triIndex of -1 indicates not in use.
    float barys[3]; // barycentric coordinates of origin for v0-v2
    float defFrame[3][3]; // default xyz axes for frame adjustment as pretransform... posttransform to (y cross z, normal, normalize(v0-origin))
	float defScale[3]; // default scalars for attached models
	float defOrg[3]; // default origin for attached models as translate adjustment
	// exterior attached entities can override the def* fields with their own attachment information.
	// mount point data is combined each frame to calculate necessary translations/rotations to and from the mount point.  The translations and
	// rotations are kept independent for simplicity of matrix inversion using the transpose.  Exterior mounts use the scale and origin values
	// as well, while internals do not.
	// all matrices are row-major (i.e. vector = vector * matrix)
	// Note: % between two vectors represents component multiply
	// translateVector = v0*barys[0] + v1*barys[1] + v2*barys[2]
	// defFrameMatrix = matrix from frame (defFrame[0], defFrame[1], defFrame[2])
	// adjustMatrix = matrix from frame ((normal of triIndex) cross (normalize(v0-translateVector)), normal of triIndex, normalize(v0-translateVector))
	// rotateMatrix = defFrameMatrix * adjustMatrix
	// inverseRotateMatrix = transpose of rotateMatrix
	// recipDefScale[x] = 1.0 / defScale[x] for all components
	// MountToWorld: Interior: p = (p * rotateMatrix) + translateVector
	//               Exterior: p = (((p % defScale) + defOrg) * rotateMatrix) + translateVector
	// WorldToMount: Interior: p = (p - translateVector) * inverseRotateMatrix
	//               Exterior: p = (((p - translateVector) * inverseRotateMatrix) - defOrg) % recipDefScale

} mdxmountpoint_t;

typedef struct
{
	char frmdChunkName[32]; // frame chunk this mounting info is used for.  Can also be reference frame.
	int32_t validBits; // bit flags indicating mount points used (1) or unused (0) in this frame.  Bit is (1 << mountIndex).  Bit 0 should ALWAYS be 0 (mount 0 reserved as origin).
					// any mount points that are used in the chunk's overall validBits but not in this frame will default to the reference frame's mount point for this index.
	float mountTranslates[32][3]; // translateVector vectors for each mount point at this frame [mount][component]
	float mountRotates[32][3][3]; // adjustMatrix matrices for each mount point at this frame, [mount][row][column] == [mount][rowvec][component]
} mdxmountframe_t;

typedef struct
{
	int32_t numMountPoints; // must be either 32 or 0, 0 meaning the rest of the chunk does not exist and no mounts exist.  Any other number is invalid.
						 // make sure to check for 0 before reading the rest of this chunk, since it might not be there!
	int32_t numFrames; // number of frames in mountFrames list
	int32_t validBits; // bit flags indicating mount points used (1) or unused (0).  Bit is (1 << mountIndex).  Bit 0 should ALWAYS be 0 (mount 0 reserved as origin).
	mdxmountpoint_t mountInfo[32]; // source mount point definitions
	mdxmountframe_t mountFrames[1]; // variable sized, precalculated mounting info for each frame, generated by the mountInfo definitions.
	                                // IMPORTANT: For this to be accurate, all vertices of a mountpoint triangle MUST have the same basis mountpoint (origin or otherwise).
} mdxmpntchunk_t;


// _____RFRM Chunk (only one per file)

typedef struct
{
	uint8_t groupNum; // group number for byte decompression
	uint8_t v[3];
	uint8_t normal[3]; // s.7, s = sign bit, .7 = fabs(component*127);
	uint8_t mountIndex; // mount point for basis
	//                  0 = none given (for FRMD chunks, use the one given for the RFRM.  For RFRM, use origin frame)
	//                  0xFF = force to none (identical to "none given" but doesn't default back to RFRM)
	//                  1 through 0xFE = use (index-1) in MPNT chunk
} mdxvert_t;

typedef struct
{
	int16_t s;
	int16_t t;
} mdxtvert_t;

typedef struct
{
	float scales[16][3]; // scales for each group number, multiply byte verts by this
	float translates[16][3]; // translates for each group number, add this after scale
    float bBox[2][16][3]; // bounding boxes min and max for each group number, [min/max][group][xyz]
} mdxframeinfo_t;

typedef struct
{
	mdxframeinfo_t frameInfo; // geometry information for frame
    int32_t numVerts; // number of model vertices
	int32_t numTris; // number of model triangles (must match numTris in TRIS chunk)
	int32_t info[1]; // variable sized, verts + baseTris + triSkins back to back
	// mdxvert_t verts[numVerts]; // vertex positions
	// mdxtvert_t baseTris[numTris][3]; // texture vert "baseframe" positions for triangles
	// byte triSkins[numTris]; // skin indices into SKIN chunk for triangles
} mdxrfrmchunk_t;


// _____FRMD Chunk

typedef struct
{
	mdxframeinfo_t frameInfo; // geometry information for frame
	int32_t triInfoOfs; // byte offset into info block of triInfo
	int32_t info[1]; // variable sized, vertInfo + triInfo back to back
	// vertInfo format: short vertex command plus any command data.  Command 0 is end of vertInfo
	//   Vertex commands: high four bits are command, low 12 bits optionally used as part of command data
	//   Command 0: End of vertInfo
	//   Command 1: Single Vert: Low 12 bits are vertex index, followed by mdxvert_t replacement for that index.
	//   Command 2: Contiguous Vert Set: Low 12 bits are starting vertex index, followed by a short count number of
	//                                   verts, followed by count number of mdxvert_t's.
	// triInfo format: short triangle command plus any command data.  Command 0 is end of triInfo
	//   Triangle commands: high four bits are command, low 12 bits optionally used as part of command data
	//   Command 0: End of triInfo
	//   Command 1: Single Tri TVerts: Low 12 bits are triangle index, followed by three mdxtvert_t's for that tri.
	//   Command 2: Multiple Tri TVerts: Low 12 bits are triCount, followed by three mdxtvert_t's to use,
	//                                   followed by triCount short triangle indices to use these tverts with.
	//   Command 3: Tri Range TVerts: Low 12 bits are starting triangle index, followed by three mdxtvert_t's to use,
	//                                followed by a short count number to use them for, beginning with the starting index.
	//   Command 4: Contiguous TVert Set: Low 12 bits are starting triangle index, followed by a short count number of
	//                                    triangles, followed by count number of three mdxtvert_t sets
	//   Command 5: Single Tri Skin: Low 12 bits are triangle index, followed by short skin index for that tri.
	//   Command 6: Multiple Tri Skin: Low 12 bits are triCount, followed by a short skin index to use,
	//                                 followed by triCount short triangle indices to use the skin with.
	//   Command 7: Tri Range Skin: Low 12 bits are starting triangle index, followed by a short skin index to use,
	//                              followed by a short count number to use it for, beginning with the starting index.
	//   Command 8: Contiguous Skin Set: Low 12 bits are starting triangle index, followed by a short count number of
	//                                   triangles, followed by count number of short skin indices
	//   Commands 9-15: Reserved for future use

} mdxfrmdchunk_t;


// _____FSEQ Chunk

// version 2
typedef struct
{
	int32_t timeStart; // beginning of range in msec
	int32_t timeDuration; // duration of range in msec
	int32_t triggerOfs; // offset into **mdxfseqchunk_t** of trigger stream, falls into triggerBuffer area.  0 for no triggers.
	int32_t flags;
	char frmdChunkName[32]; // frame chunk name at start of range.  Can also be reference frame.
} mdxSeqBlock_v2_t;

typedef struct
{
	int32_t numBlocks;
	mdxSeqBlock_v2_t blocks[1]; // variable sized
	// following the blocks is a buffer for data referred to by triggerOfs
} mdxfseqchunk_v2_t;

// version 3
typedef struct
{
	char frmdChunkName[32]; // frame chunk name at start of range.  Can also be reference frame.  If empty, means trigger.
	float triggerTimeFrac; // time in sequence from 0.0-1.0 for trigger (only used if trigger)
	int32_t triggerOfs; // offset into **mdxfseqchunk_t** of trigger stream, falls into triggerBuffer area.  0 for no triggers.
} mdxSeqBlock_v3_t;

typedef struct
{
	float framesPerSecond; // rate of animation in frames per second
	int32_t numBlocks; // number of blocks
	mdxSeqBlock_v3_t blocks[1]; // variable sized
	// following the blocks is a buffer for data referred to by triggerOfs
} mdxfseqchunk_v3_t;

//----------------------------------------------------------------------------
//    Public Data Declarations
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//    Public Function Declarations
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//    Class Headers
//----------------------------------------------------------------------------


//****************************************************************************
//**
//**    END HEADER MDX_MAN.H
//**
//****************************************************************************
#pragma pack(pop)
#endif // __MDX_MAN_H__
