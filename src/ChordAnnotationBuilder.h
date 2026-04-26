#ifndef __CHORD_ANNOTATION_BUILDER_H__
#define __CHORD_ANNOTATION_BUILDER_H__

#include "BarMap.h"
#include "ChordAnnotation.h"
#include "SongData.h"

QVector<ChordAnnotation> buildChordAnnotations(const SongData& song, const BarMap& bars,
                                               ChordAnnotationOptions options = ChordAnnotationOptions());

#endif
