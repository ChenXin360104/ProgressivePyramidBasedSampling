#include "BinningTree.h"

TreeParams tree_sampling_params = { 0.003, 0.013, 4 };

using namespace std;

BinningTree::BinningTree(const FilteredPointSet* origin, const QRect& bounding_rect)
	: horizontal_bin_num(bounding_rect.width() / params.grid_width + 1), vertical_bin_num(bounding_rect.height() / params.grid_width + 1), margin_left(bounding_rect.left()), margin_top(bounding_rect.top())
{
	// create all min grids
	dataset = make_unique<FilteredPointSet>();
	updateMinGrids(origin);
}

bool BinningTree::split(std::shared_ptr<BinningTreeNode> node)
{
	if (node->min_grids_inside.size() < 2)
		return false;

	double x_sum = 0.0, y_sum = 0.0;
	uint x_min = -1, y_min = -1, x_max = 0, y_max = 0;
	for (auto ptr : node->min_grids_inside) {
		auto s_ptr = ptr.lock();
		x_sum += s_ptr->left * s_ptr->contents.size();
		y_sum += s_ptr->top * s_ptr->contents.size();
		x_min = min(x_min, s_ptr->left);
		y_min = min(y_min, s_ptr->top);
		x_max = max(x_max, s_ptr->left);
		y_max = max(y_max, s_ptr->top);
	}
	uint split_width = (uint)ceil(x_sum / node->info.total_num - node->b.left), split_height = (uint)ceil(y_sum / node->info.total_num - node->b.top);
	uint x_split = split_width + node->b.left, y_split = split_height + node->b.top;

	vector<weak_ptr<MinGrid>> child1_vec, child2_vec;
	if ((x_max - x_min) > (y_max-y_min) && splitHelper(node, [=](shared_ptr<MinGrid> ptr) { return ptr->left < x_split; }, &child1_vec, &child2_vec)) { // horizontal split
		node->child1 = make_shared<BinningTreeNode>(move(Box(node->b.left, node->b.top, split_width, node->b.height)), move(child1_vec), countStatisticalInfo(child1_vec), node);
		node->child2 = make_shared<BinningTreeNode>(move(Box(x_split, node->b.top, node->b.width - split_width, node->b.height)), move(child2_vec), countStatisticalInfo(child2_vec), node);
	}
	else if (splitHelper(node, [=](shared_ptr<MinGrid> ptr) { return ptr->top < y_split; }, &child1_vec, &child2_vec)) { // vertical split
		node->child1 = make_shared<BinningTreeNode>(move(Box(node->b.left, node->b.top, node->b.width, split_height)), move(child1_vec), countStatisticalInfo(child1_vec), node);
		node->child2 = make_shared<BinningTreeNode>(move(Box(node->b.left, y_split, node->b.width, node->b.height - split_height)), move(child2_vec), countStatisticalInfo(child2_vec), node);
	}
	else {
		return false;
	}
	return true;
}

bool BinningTree::split_new(std::shared_ptr<BinningTreeNode> node)
{
	if (node->min_grids_inside.size() < 2)
		return false;

	double x_sum = 0.0, y_sum = 0.0;
	for (auto ptr : node->min_grids_inside) {
		auto s_ptr = ptr.lock();
		x_sum += s_ptr->left * s_ptr->contents.size();
		y_sum += s_ptr->top * s_ptr->contents.size();
	}
	uint split_width = (uint)floor(x_sum / node->info.total_num - node->b.left), split_height = (uint)floor(y_sum / node->info.total_num - node->b.top);
	uint x_split = split_width + node->b.left, y_split = split_height + node->b.top;

	// find best splitting line
	int x_split_left_sum = 0, x_split_middle_sum = 0, x_split_right_sum = 0, y_split_left_sum = 0, y_split_middle_sum = 0, y_split_right_sum = 0;
	for (auto ptr : node->min_grids_inside) {
		auto s_ptr = ptr.lock();
		if (s_ptr->left < x_split)
			x_split_left_sum += s_ptr->contents.size();
		else if (s_ptr->left == x_split)
			x_split_middle_sum += s_ptr->contents.size();
		else
			x_split_right_sum += s_ptr->contents.size();
		if (s_ptr->top < y_split)
			y_split_left_sum += s_ptr->contents.size();
		else if (s_ptr->top == y_split)
			y_split_middle_sum += s_ptr->contents.size();
		else
			y_split_right_sum += s_ptr->contents.size();
	}
	int x_diff = abs(x_split_left_sum - x_split_middle_sum - x_split_right_sum), y_diff = abs(y_split_left_sum - y_split_middle_sum - y_split_right_sum),
		x_diff2 = abs(x_split_left_sum + x_split_middle_sum - x_split_right_sum), y_diff2 = abs(y_split_left_sum + y_split_middle_sum - y_split_right_sum);
	if (x_diff > x_diff2) {
		++split_width;
		++x_split;
		x_diff = x_diff2;
	}
	if (y_diff > y_diff2) {
		++split_height;
		++y_split;
		y_diff = y_diff2;
	}

	vector<weak_ptr<MinGrid>> child1_vec, child2_vec;
	if ((x_diff < y_diff) && splitHelper(node, [=](shared_ptr<MinGrid> ptr) { return ptr->left < x_split; }, &child1_vec, &child2_vec)) { // horizontal split
		node->child1 = make_shared<BinningTreeNode>(move(Box(node->b.left, node->b.top, split_width, node->b.height)), move(child1_vec), countStatisticalInfo(child1_vec), node);
		node->child2 = make_shared<BinningTreeNode>(move(Box(x_split, node->b.top, node->b.width - split_width, node->b.height)), move(child2_vec), countStatisticalInfo(child2_vec), node);
	}
	else if (splitHelper(node, [=](shared_ptr<MinGrid> ptr) { return  ptr->top < y_split; }, &child1_vec, &child2_vec)) { // vertical split
		node->child1 = make_shared<BinningTreeNode>(move(Box(node->b.left, node->b.top, node->b.width, split_height)), move(child1_vec), countStatisticalInfo(child1_vec), node);
		node->child2 = make_shared<BinningTreeNode>(move(Box(node->b.left, y_split, node->b.width, node->b.height - split_height)), move(child2_vec), countStatisticalInfo(child2_vec), node);
	}
	else {
		return false;
	}
	return true;
}

bool BinningTree::splitHelper(shared_ptr<BinningTreeNode> node, function<bool(shared_ptr<MinGrid>)> comp, vector<weak_ptr<MinGrid>> *child1_vec, vector<weak_ptr<MinGrid>> *child2_vec)
{
	child1_vec->clear();
	child2_vec->clear();

	for (auto ptr : node->min_grids_inside) {
		auto s_ptr = ptr.lock();
		if (comp(s_ptr))
			child1_vec->push_back(ptr);
		else
			child2_vec->push_back(ptr);
	}
	if(child1_vec->empty() || child2_vec->empty())
		return false;

	return true;
}

uint BinningTree::selectSeedIndex(shared_ptr<BinningTreeNode> node)
{
	vector<uint> indices;
	for (auto &b : node->min_grids_inside) {
		indices.push_back(b.lock()->contents.front());
	}

	node->seed_index = indices[rand() % indices.size()];
	return node->seed_index;
}

uint BinningTree::selectSeedIndex(shared_ptr<BinningTreeNode> node, uint label)
{
	vector<uint> indices;
	for (auto &b : node->min_grids_inside) {
		//for (uint i : b.lock()->contents) {
		//	if (dataset->at(i)->label == label) {
		//		indices.push_back(i);
		//	}
		//}
		indices.push_back(b.lock()->contents.front());
	}

	node->seed_index = indices[rand() % indices.size()];
	return node->seed_index;
}

void BinningTree::updateLeafNum(shared_ptr<BinningTreeNode> node)
{
	node->leaf_num_inside = node->child1->leaf_num_inside + node->child2->leaf_num_inside;
}

NodeWithQuota BinningTree::backtrack(shared_ptr<BinningTreeNode> leaf, uint max_depth)
{
	if (leaf->info.class_point_num.size() == 1) // only 1 class in the bin
		return make_pair(leaf, unordered_map<uint, size_t>{ {leaf->info.class_point_num.begin()->first, 1} });

	NodeWithQuota result;
	shared_ptr<BinningTreeNode> current = leaf;
	double min_diff = DBL_MAX;
	while ((current = current->parent.lock()) && max_depth--) {
		auto &&quota = determineQuota(current);
		auto &cpn = current->info.class_point_num;
		if (current->leaf_num_inside < cpn.size()) {
			result = make_pair(current, quota);
		}
		else {
			double diff = 0.0;
			vector<uint> class_;
			for (auto &pr : cpn) {
				class_.push_back(pr.first);
			}
			vector<bool> v(class_.size());
			fill(v.begin(), v.begin() + 2, true);
			do {
				vector<int> class_indices;
				for (int i = 0; i < v.size(); ++i) {
					if (v[i]) {
						class_indices.push_back(class_[i]);
					}
				}
				if(cpn[class_indices[0]] < cpn[class_indices[1]])
					diff += abs((double)quota[class_indices[0]] / quota[class_indices[1]] - (double)cpn[class_indices[0]] / cpn[class_indices[1]]);
				else
					diff += abs((double)quota[class_indices[1]] / quota[class_indices[0]] - (double)cpn[class_indices[1]] / cpn[class_indices[0]]);
			} while (std::prev_permutation(v.begin(), v.end()));

			if (diff < min_diff) {
				min_diff = diff;
				result = make_pair(current, quota);
			}
		}
	}
	return result;
}

void BinningTree::updateMinGrids(const FilteredPointSet * origin)
{
	vector<weak_ptr<MinGrid>> vec;
	for (auto &pr : min_grids) {
		vec.push_back(pr.second);
	}
	for (auto &pr : *origin) {
		auto &p = pr.second;
		int x = visual2grid(p->pos.x(), margin_left),
			y = visual2grid(p->pos.y(), margin_top);
		auto &pos = make_pair(x, y);
		if (min_grids.find(pos) == min_grids.end()) {
			min_grids[pos] = make_shared<MinGrid>(x, y);
			vec.push_back(min_grids[pos]);
			dataset->emplace(pr.first, make_unique<LabeledPoint>(pr.second)); // when selecting seeds, the position is always the first one in *contents*
		}
		else if (double_dist(gen) < 0.1) {
			dataset->at(min_grids[pos]->contents.front())->label = pr.second->label;
		}
		min_grids[pos]->contents.push_back(pr.first);
		++grid_infos[pos].total_num;
		++grid_infos[pos].class_point_num[p->label];
	}
	vec.shrink_to_fit();
	StatisticalInfo info = countStatisticalInfo(vec);

	// create root of the tree
	Box root_bin(0, 0, horizontal_bin_num, vertical_bin_num);
	root = make_shared<BinningTreeNode>(move(root_bin), move(vec), move(info), weak_ptr<BinningTreeNode>());
}

void BinningTree::fillQuota(uint remaining_leaf_num, const unordered_map<uint, size_t> &label_point_map, unordered_map<uint, size_t>& quota, const function<size_t(uint)>& getAmount)
{
	vector<uint> labels;
	vector<size_t> class_point_num;
	for (auto &u : label_point_map) {
		labels.push_back(u.first);
		if (quota.find(u.first) != quota.end())
			class_point_num.push_back(getAmount(u.first));
		else
			class_point_num.push_back(u.second);
	}

	vector<uint> indices(class_point_num.size());
	iota(indices.begin(), indices.end(), 0);
	auto& getScore = [&class_point_num](uint i) { return static_cast<double>(class_point_num[i]); };

	while (remaining_leaf_num--) {
		uint i = rouletteSelection(indices, getScore);
		++quota[labels[i]];
		--class_point_num[i];
	}
}

unordered_map<uint, size_t> BinningTree::determineQuota(shared_ptr<BinningTreeNode> node)
{
	unordered_map<uint, size_t> quota;
	for (auto &u : node->info.class_point_num) { // initialization
		if (node->leaf_num_inside == quota.size())
			break;
		quota[u.first] = 1;
	}

	fillQuota(node->leaf_num_inside - quota.size(), node->info.class_point_num, quota, [node, &quota](uint key) {return node->info.class_point_num[key] - quota[key]; });
	return quota;
}

unordered_map<uint, size_t> computeQuota(vector<NodeWithQuota>* leaves)
{
	unordered_map<uint, size_t> quota;
	for (auto &l : *leaves) {
		auto &it = l.second.begin();
		quota[it->first]++;
	}
	return quota;
}

void BinningTree::splitQuota(const NodeWithQuota& node, vector<NodeWithQuota>* leaves)
{
	shared_ptr<BinningTreeNode> self = node.first.lock();
	if (!self->child1) { // is leaf
		leaves->push_back(node);
		return;
	}

	shared_ptr<BinningTreeNode> inadequate, another;
	if (self->child1->info.class_point_num.size() > self->child2->info.class_point_num.size()) {
		inadequate = self->child1;
		another = self->child2;
	}
	else {
		inadequate = self->child2;
		another = self->child1;
	}

	priority_queue<pair<uint, size_t>, vector<pair<uint, size_t>>, ClassPointComparator<size_t>> q;
	auto &map_in_inadequate = inadequate->info.class_point_num, &map_in_another = another->info.class_point_num;
	for (auto &u : node.second) {
		if (map_in_another.find(u.first) == map_in_another.end())
			q.push(*map_in_inadequate.find(u.first));
	}

	auto &pushKMaxToMap = [](uint k, decltype(q) &q, unordered_map<uint, size_t>& map)
	{
		while (k-- && !q.empty()) {
			map[q.top().first] = 1;
			q.pop();
		}
	};
	unordered_map<uint, size_t> inadequate_quato;
	if (q.size() > inadequate->leaf_num_inside) {
		pushKMaxToMap(inadequate->leaf_num_inside, q, inadequate_quato);
	}
	else {
		size_t remain = inadequate->leaf_num_inside - q.size();
		pushKMaxToMap(q.size(), q, inadequate_quato);
		fillQuota(remain, map_in_inadequate, inadequate_quato, [node, &map_in_inadequate, &inadequate_quato](uint key) {
			size_t amount = map_in_inadequate[key] - inadequate_quato[key];
			if (node.second.find(key) != node.second.end())
				return min(amount, node.second.at(key));
			else
				return (size_t) 0;
		});
	}
	splitQuota(make_pair(inadequate, inadequate_quato), leaves);

	unordered_map<uint, size_t> another_quato;
	uint remaining_leaf_num = another->leaf_num_inside;
	for (auto &u : node.second) {
		another_quato[u.first] = u.second;
		if (inadequate_quato.find(u.first) != inadequate_quato.end()) // exists in the inadequate bin
			another_quato[u.first] -= inadequate_quato[u.first];
		if (another_quato[u.first] == 0 || map_in_another.find(u.first) == map_in_another.end()) // not in the another bin
			another_quato.erase(u.first);
		else if (another_quato[u.first] > remaining_leaf_num) {
			another_quato[u.first] = remaining_leaf_num;
			remaining_leaf_num = 0;
			break;
		}
		else
			remaining_leaf_num -= another_quato[u.first];
	}
	if (remaining_leaf_num > 0) {
		fillQuota(remaining_leaf_num, map_in_another, another_quato, [&map_in_another, &another_quato](uint key) {return map_in_another[key] - another_quato[key]; });
	}

	splitQuota(make_pair(another, another_quato), leaves);
}

StatisticalInfo BinningTree::countStatisticalInfo(const vector<weak_ptr<MinGrid>>& vec)
{
	StatisticalInfo result;
	for (auto ptr : vec) {
		auto idx = make_pair(ptr.lock()->left, ptr.lock()->top);
		result.total_num += grid_infos[idx].total_num;
		for (auto &pr : grid_infos[idx].class_point_num) {
			result.class_point_num[pr.first] += pr.second;
		}
	}
	return result;
}

uint BinningTree::rouletteSelection(const vector<uint>& items, std::function<double(uint)> scoreFunc)
{
	vector<double> scores;
	double sum_score = 0.0;
	for (auto i : items) {
		scores.push_back(scoreFunc(i));
		sum_score += scores.back();
	}
	if (sum_score == 0.0) return 0;

	uniform_real_distribution<double> unif(0.0, sum_score);
	double val = unif(gen);
	size_t i = 0, sz = items.size();
	for (; i < sz; ++i) {
		val -= scores[i];
		if (val < 0.0)
			break;
	}
	return i;
}
