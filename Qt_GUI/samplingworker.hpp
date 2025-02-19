﻿#pragma once

#include "global.h"
#include "utils.h"
#include "HierarchicalSampling.h"
#include "AdaptiveBinningSampling.h"
#include "ReservoirSampling.h"
#include "RandomSampling.h"

class SamplingWorker : public QObject {
	Q_OBJECT

public:
	SamplingWorker() { openDataSource(data_source, MY_DATASET_FILENAME); }
	uint getPointCount() { return point_count; }
	const std::vector<uint>& getSelected() { return seeds; }
	PointSet getSeedsOfSpecificFrame() { return hs.getSeeds(); }

	// open a input stream with the given path
	void setDataSource(const std::string& data_path);
	void setClassMapping(std::unordered_map<uint, std::string>* class_mapping) { class2label = class_mapping; }
	// recreate the density maps with the latest grid size for HierarchicalSampling
	void updateGrids();

public slots:
	void readAndSample();

signals:
	void readFinished(FilteredPointSet* filtered_points);
	void sampleFinished(std::pair<PointSet, PointSet>* removed_n_added);
	void writeFrame(int frame_id);
	void finished();

private:
	HierarchicalSampling hs{ QRect(MARGIN.left, MARGIN.top, CANVAS_WIDTH - MARGIN.left - MARGIN.right, CANVAS_HEIGHT - MARGIN.top - MARGIN.bottom) };
	ReservoirSampling rs;
	AdaptiveBinningSampling abs;
	RandomSampling rands;

	Indices seeds;
	uint point_count = 0;
	FilteredPointSet* _filtered_new_data = nullptr; // used to draw 
	std::pair<PointSet, PointSet>* _result = nullptr;

	std::unordered_map<uint, std::string>* class2label;
	std::ifstream data_source;
	Extent real_extent, visual_extent = { (qreal)MARGIN.left, (qreal)MARGIN.top, (qreal)(CANVAS_WIDTH - MARGIN.right), (qreal)(CANVAS_HEIGHT - MARGIN.bottom) };
};
