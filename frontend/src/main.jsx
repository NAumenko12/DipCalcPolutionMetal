import React, { useEffect, useMemo, useRef, useState } from "react";
import { createRoot } from "react-dom/client";
import L from "leaflet";
import "leaflet/dist/leaflet.css";
import "./styles.css";

const SOURCE_POINT = { latitude: 67.92384, longitude: 32.840962 };
const initialApiBase = localStorage.getItem("testdip.apiBase") || window.location.origin;

function App() {
  const [activeView, setActiveView] = useState("map");
  const [apiBase, setApiBase] = useState(initialApiBase);
  const [apiDraft, setApiDraft] = useState(initialApiBase);
  const [apiStatus, setApiStatus] = useState("проверка...");
  const [locations, setLocations] = useState([]);
  const [metals, setMetals] = useState([]);
  const [samples, setSamples] = useState([]);
  const [statistics, setStatistics] = useState([]);
  const [gridPoints, setGridPoints] = useState([]);
  const [jobs, setJobs] = useState([]);
  const [selectedJobId, setSelectedJobId] = useState("");
  const [selectedJobPoints, setSelectedJobPoints] = useState([]);
  const [selectedLocationId, setSelectedLocationId] = useState("");
  const [sampleFilters, setSampleFilters] = useState({ metalId: "", year: "" });
  const [statsFilters, setStatsFilters] = useState({ locationId: "", metalId: "", yearFrom: "", yearTo: "" });
  const [calculation, setCalculation] = useState({ metalId: "", year: "2014", gridStepKm: "0.7", areaSizeKm: "200" });
  const [currentJob, setCurrentJob] = useState(null);
  const [dialog, setDialog] = useState(null);
  const [toast, setToast] = useState("");
  const mapRef = useRef(null);

  const api = useMemo(() => createApiClient(apiBase), [apiBase]);
  const selectedLocation = locations.find((item) => Number(item.id) === Number(selectedLocationId)) || locations[0];

  const showToast = (message) => {
    setToast(message);
    window.setTimeout(() => setToast(""), 3200);
  };

  const loadHealth = async () => {
    try {
      await api.get("/health");
      setApiStatus("online");
    } catch {
      setApiStatus("offline");
    }
  };

  const loadBaseData = async () => {
    const [locationData, metalData] = await Promise.all([
      api.get("/api/locations"),
      api.get("/api/metals")
    ]);
    const nextLocations = locationData.items || [];
    const nextMetals = metalData.items || [];
    setLocations(nextLocations);
    setMetals(nextMetals);
    setSelectedLocationId((current) => current || nextLocations[0]?.id || "");
    setCalculation((current) => ({
      ...current,
      metalId: current.metalId || nextMetals[0]?.id || ""
    }));
  };

  const loadSamples = async () => {
    const locationId = selectedLocationId || locations[0]?.id;
    if (!locationId) {
      setSamples([]);
      return;
    }
    const params = new URLSearchParams({ limit: "500" });
    if (sampleFilters.metalId) params.set("metalId", sampleFilters.metalId);
    if (sampleFilters.year) params.set("year", sampleFilters.year);
    const page = await api.get(`/api/locations/${locationId}/samples?${params.toString()}`);
    setSamples(page.items || []);
  };

  const loadStatistics = async () => {
    const params = new URLSearchParams();
    const locationId = statsFilters.locationId || selectedLocationId;
    const metalId = statsFilters.metalId || sampleFilters.metalId;
    const yearTo = statsFilters.yearTo || sampleFilters.year;
    if (locationId) params.set("locationId", locationId);
    if (metalId) params.set("metalId", metalId);
    if (statsFilters.yearFrom) params.set("yearFrom", statsFilters.yearFrom);
    if (yearTo) params.set("yearTo", yearTo);
    const data = await api.get(`/api/statistics/samples?${params.toString()}`);
    setStatistics(data.items || []);
  };

  const loadPreviewPoints = async () => {
    const data = await api.get("/api/calculations/preview");
    setGridPoints(data.items || []);
  };

  const loadJobs = async () => {
    const data = await api.get("/api/calculations?limit=100");
    const items = data.items || [];
    setJobs(items);
    setSelectedJobId((current) => current || items[0]?.id || "");
  };

  useEffect(() => {
    loadHealth();
    Promise.all([loadBaseData(), loadJobs()]).catch((error) => showToast(error.message));
  }, [apiBase]);

  useEffect(() => {
    if (!locations.length) return;
    loadSamples().catch((error) => showToast(error.message));
    loadStatistics().catch((error) => showToast(error.message));
  }, [selectedLocationId, sampleFilters, statsFilters, locations.length]);

  useEffect(() => {
    loadPreviewPoints().catch(() => setGridPoints([]));
  }, [apiBase]);

  const saveApiBase = () => {
    const next = apiDraft.trim() || window.location.origin;
    localStorage.setItem("testdip.apiBase", next);
    setApiBase(next);
  };

  const selectLocation = (id) => {
    setSelectedLocationId(id);
    setStatsFilters((current) => ({ ...current, locationId: id }));
  };

  const submitCalculation = async (event) => {
    event.preventDefault();
    try {
      const payload = {
        referenceLocationId: Number(selectedLocationId || locations[0]?.id),
        metalId: Number(calculation.metalId),
        year: Number(calculation.year),
        gridStepKm: Number(calculation.gridStepKm),
        areaSizeKm: Number(calculation.areaSizeKm)
      };
      const job = await api.post("/api/calculations/concentration", payload);
      setCurrentJob(job);
      setSelectedJobId(job.id);
      showToast("Расчет поставлен в очередь");
      pollJob(job.id);
    } catch (error) {
      showToast(`Расчет не запущен: ${error.message}`);
    }
  };

  const pollJob = async (jobId) => {
    for (let attempt = 0; attempt < 20; attempt += 1) {
      await delay(1200);
      const job = await api.get(`/api/calculations/${jobId}`);
      setCurrentJob(job);
      setSelectedJobId(job.id);
      if (["completed", "failed", "publish_failed"].includes(job.status)) break;
    }
    loadJobs().catch(() => {});
  };

  const loadJobPoints = async (jobId = currentJob?.id || selectedJobId) => {
    if (!jobId) {
      showToast("Сначала запусти расчет");
      return;
    }
    const [job, data] = await Promise.all([
      api.get(`/api/calculations/${jobId}`),
      api.get(`/api/calculations/${jobId}/points`)
    ]);
    const points = data.items || [];
    setCurrentJob(job);
    setSelectedJobId(job.id);
    setSelectedJobPoints(points);
    setGridPoints(points);
    setActiveView("map");
    showToast(`Загружено точек поля: ${data.count || 0}`);
  };

  const selectJob = async (jobId) => {
    if (!jobId) return;
    setSelectedJobId(jobId);
    const job = await api.get(`/api/calculations/${jobId}`);
    setCurrentJob(job);
    try {
      const data = await api.get(`/api/calculations/${jobId}/points`);
      setSelectedJobPoints(data.items || []);
    } catch {
      setSelectedJobPoints([]);
    }
  };

  const saveLocation = async (location) => {
    if (location.id) await api.put(`/api/locations/${location.id}`, location);
    else await api.post("/api/locations", location);
    setDialog(null);
    await loadBaseData();
    showToast("Площадка сохранена");
  };

  const deleteLocation = async () => {
    if (!selectedLocation || !confirm(`Удалить площадку ${selectedLocation.siteNumber}?`)) return;
    await api.delete(`/api/locations/${selectedLocation.id}`);
    setSelectedLocationId("");
    await loadBaseData();
    showToast("Площадка удалена");
  };

  const saveMetal = async (metal) => {
    if (metal.id) await api.put(`/api/metals/${metal.id}`, metal);
    else await api.post("/api/metals", metal);
    setDialog(null);
    await loadBaseData();
    showToast("Металл сохранен");
  };

  const deleteMetal = async (metal) => {
    if (!metal || !confirm(`Удалить металл ${metal.symbol || metal.name}? Связанные пробы тоже будут удалены.`)) return;
    await api.delete(`/api/metals/${metal.id}`);
    await loadBaseData();
    await loadSamples();
    await loadStatistics();
    showToast("Металл удален");
  };

  const saveSample = async (sample) => {
    if (sample.id) await api.put(`/api/samples/${sample.id}`, sample);
    else await api.post(`/api/locations/${sample.locationId}/samples`, sample);
    setDialog(null);
    await loadSamples();
    await loadStatistics();
    showToast("Проба сохранена");
  };

  const importSamplesCsv = async (file) => {
    if (!file) return;
    const rows = parseCsv(await file.text());
    const header = rows.shift()?.map((item) => item.trim()) || [];
    const missing = ["locationId", "metalId", "value", "samplingDate"].filter((name) => !header.includes(name));
    if (missing.length) {
      showToast(`CSV: нет колонок ${missing.join(", ")}`);
      return;
    }
    let imported = 0;
    for (const row of rows) {
      const record = Object.fromEntries(header.map((name, index) => [name, row[index] || ""]));
      const payload = {
        locationId: Number(record.locationId),
        metalId: Number(record.metalId),
        value: Number(record.value),
        samplingDate: record.samplingDate,
        position: record.position || "",
        repetition: Number(record.repetition || 1),
        analyticsNumber: record.analyticsNumber || ""
      };
      await api.post(`/api/locations/${payload.locationId}/samples`, payload);
      imported += 1;
    }
    await loadSamples();
    await loadStatistics();
    showToast(`Импортировано проб: ${imported}`);
  };

  const exportSamplesCsv = () => {
    const rows = [["id", "metal", "value", "samplingDate", "position", "repetition"]];
    samples.forEach((sample) => rows.push([
      sample.id,
      metalName(metals, sample.metalId),
      sample.value,
      sample.samplingDate,
      sample.position || "",
      sample.repetition || 1
    ]));
    downloadCsv(rows, "testdip-samples.csv");
  };

  const exportMapPng = () => {
    const canvas = document.createElement("canvas");
    canvas.width = 1200;
    canvas.height = 750;
    drawStaticMap(canvas.getContext("2d"), canvas, locations, gridPoints, selectedLocationId);
    canvas.toBlob((blob) => downloadBlob(blob, "testdip-map.png"));
  };

  return (
    <div className="app-shell">
      <Sidebar
        activeView={activeView}
        setActiveView={setActiveView}
      />
      <main className="workspace">
        <header className="topbar">
          <div>
            <h1>Расчет загрязнения тяжелыми металлами</h1>
            <p>Карта пробоотбора, данные концентраций, статистика и результаты расчетов.</p>
          </div>
          <div className="top-actions">
            <button title="Обновить данные" onClick={() => loadBaseData().then(() => showToast("Данные обновлены"))}>↻</button>
            <button title="Экспорт текущей таблицы в CSV" onClick={exportSamplesCsv}>CSV</button>
            <button title="Экспорт карты в PNG" onClick={exportMapPng}>PNG</button>
          </div>
        </header>

        {activeView === "map" && (
          <MapView
            mapRef={mapRef}
            locations={locations}
            gridPoints={gridPoints}
            selectedLocationId={selectedLocationId}
            selectLocation={selectLocation}
            selectedLocation={selectedLocation}
            editLocation={() => setDialog({ type: "location", value: selectedLocation })}
            deleteLocation={() => deleteLocation().catch((error) => showToast(error.message))}
          />
        )}

        {activeView === "data" && (
          <DataView
            locations={locations}
            metals={metals}
            samples={samples}
            selectedLocationId={selectedLocationId}
            sampleFilters={sampleFilters}
            setSampleFilters={setSampleFilters}
            selectLocation={selectLocation}
            openLocation={() => setDialog({ type: "location", value: null })}
            openSample={(sample) => setDialog({ type: "sample", value: sample || null })}
            importSamplesCsv={(file) => importSamplesCsv(file).catch((error) => showToast(`CSV: ${error.message}`))}
          />
        )}

        {activeView === "calc" && (
          <CalcView
            locations={locations}
            metals={metals}
            selectedLocationId={selectedLocationId}
            selectLocation={selectLocation}
            calculation={calculation}
            setCalculation={setCalculation}
            submitCalculation={submitCalculation}
            currentJob={currentJob}
            loadJobPoints={() => loadJobPoints().catch((error) => showToast(error.message))}
          />
        )}

        {activeView === "details" && (
          <CalculationDetailsView
            jobs={jobs}
            selectedJobId={selectedJobId}
            currentJob={currentJob}
            selectedJobPoints={selectedJobPoints}
            metals={metals}
            locations={locations}
            selectJob={(jobId) => selectJob(jobId).catch((error) => showToast(error.message))}
            loadJobPoints={(jobId) => loadJobPoints(jobId).catch((error) => showToast(error.message))}
          />
        )}

        {activeView === "reports" && (
          <ReportsView
            jobs={jobs}
            selectedJobId={selectedJobId}
            currentJob={currentJob}
            selectedJobPoints={selectedJobPoints}
            statistics={statistics}
            metals={metals}
            locations={locations}
            selectJob={(jobId) => selectJob(jobId).catch((error) => showToast(error.message))}
          />
        )}

        {activeView === "refs" && (
          <ReferencesView
            locations={locations}
            metals={metals}
            calculation={calculation}
            selectLocation={selectLocation}
            selectedLocationId={selectedLocationId}
            openLocation={() => setDialog({ type: "location", value: null })}
            openMetal={(metal) => setDialog({ type: "metal", value: metal || null })}
            deleteMetal={(metal) => deleteMetal(metal).catch((error) => showToast(error.message))}
          />
        )}

        {activeView === "stats" && (
          <StatsView
            locations={locations}
            metals={metals}
            statistics={statistics}
            statsFilters={statsFilters}
            setStatsFilters={setStatsFilters}
            loadStatistics={() => loadStatistics().catch((error) => showToast(error.message))}
          />
        )}
      </main>

      {dialog?.type === "location" && (
        <LocationDialog
          location={dialog.value}
          onClose={() => setDialog(null)}
          onSave={(value) => saveLocation(value).catch((error) => showToast(error.message))}
        />
      )}
      {dialog?.type === "sample" && (
        <SampleDialog
          sample={dialog.value}
          locations={locations}
          metals={metals}
          selectedLocationId={selectedLocationId}
          onClose={() => setDialog(null)}
          onSave={(value) => saveSample(value).catch((error) => showToast(error.message))}
        />
      )}
      {dialog?.type === "metal" && (
        <MetalDialog
          metal={dialog.value}
          onClose={() => setDialog(null)}
          onSave={(value) => saveMetal(value).catch((error) => showToast(error.message))}
        />
      )}
      <div className={`toast ${toast ? "is-visible" : ""}`} role="status">{toast}</div>
    </div>
  );
}

function Sidebar({ activeView, setActiveView }) {
  const tabs = [
    ["map", "⌖", "Карта"],
    ["data", "▤", "Данные"],
    ["calc", "∑", "Расчеты"],
    ["details", "◷", "Детали"],
    ["reports", "⇩", "Отчеты"],
    ["refs", "☷", "Справочники"],
    ["stats", "⌁", "Статистика"]
  ];
  return (
    <aside className="sidebar">
      <div className="brand">
        <span className="brand-mark">T</span>
        <div><strong>TESTDIP</strong><span>СЕВЕРОНИКЕЛЬ</span></div>
      </div>
      <nav className="nav-tabs" aria-label="Разделы">
        {tabs.map(([id, icon, label]) => (
          <button key={id} className={`nav-tab ${activeView === id ? "is-active" : ""}`} onClick={() => setActiveView(id)}>
            {icon} <span>{label}</span>
          </button>
        ))}
      </nav>
    </aside>
  );
}

function MapView({ mapRef, locations, gridPoints, selectedLocationId, selectLocation, selectedLocation, editLocation, deleteLocation }) {
  return (
    <section className="view is-active">
      <div className="map-layout">
        <section className="map-panel">
          <div className="panel-head">
            <div><h2>Пробные площадки</h2><span>{locations.length} площадок · {gridPoints.length} точек поля</span></div>
          </div>
          <LeafletMap locations={locations} gridPoints={gridPoints} selectedLocationId={selectedLocationId} selectLocation={selectLocation} />
          <div className="legend">
            <span><i className="plant" /> источник выбросов</span>
            <span><i className="location" /> пробная площадка</span>
            <span><i className="heat" /> поле концентрации</span>
          </div>
        </section>
        <aside className="details-panel">
          <h2>Выбранная площадка</h2>
          <Details rows={[
            ["ID", selectedLocation?.id],
            ["Название", selectedLocation?.name],
            ["Номер", selectedLocation?.siteNumber],
            ["Расстояние", selectedLocation ? `${formatNumber(selectedLocation.distanceFromSourceKm, 2)} км` : ""],
            ["Координаты", selectedLocation ? `${formatNumber(selectedLocation.latitude, 5)}, ${formatNumber(selectedLocation.longitude, 5)}` : ""],
            ["Описание", selectedLocation?.description || "нет описания"]
          ]} />
          <div className="compact-actions">
            <button onClick={editLocation}>Редактировать</button>
            <button className="danger-button" onClick={deleteLocation}>Удалить</button>
          </div>
        </aside>
      </div>
    </section>
  );
}

function LeafletMap({ locations, gridPoints, selectedLocationId, selectLocation }) {
  const containerRef = useRef(null);
  const mapRef = useRef(null);
  const layerRef = useRef(null);

  useEffect(() => {
    if (!containerRef.current || mapRef.current) return;
    const map = L.map(containerRef.current, { zoomControl: true }).setView([SOURCE_POINT.latitude, SOURCE_POINT.longitude], 9);
    L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
      attribution: "&copy; OpenStreetMap contributors",
      maxZoom: 18
    }).addTo(map);
    layerRef.current = L.layerGroup().addTo(map);
    mapRef.current = map;
    return () => {
      map.remove();
      mapRef.current = null;
      layerRef.current = null;
    };
  }, []);

  useEffect(() => {
    const map = mapRef.current;
    const layer = layerRef.current;
    if (!map || !layer) return;
    layer.clearLayers();
    const bounds = [];
    const addToBounds = (point) => bounds.push([Number(point.latitude), Number(point.longitude)]);

    L.circleMarker([SOURCE_POINT.latitude, SOURCE_POINT.longitude], {
      radius: 10,
      color: "#ffffff",
      weight: 3,
      fillColor: "#b84a3a",
      fillOpacity: 1
    }).bindPopup("<strong>СЕВЕРОНИКЕЛЬ</strong><br/>Источник выбросов").addTo(layer);
    addToBounds(SOURCE_POINT);

    locations.filter(validGeo).forEach((location) => {
      const selected = Number(location.id) === Number(selectedLocationId);
      L.circleMarker([Number(location.latitude), Number(location.longitude)], {
        radius: selected ? 9 : 7,
        color: "#ffffff",
        weight: 2,
        fillColor: selected ? "#0c5f55" : "#157a6e",
        fillOpacity: 0.96
      })
        .bindPopup(`<strong>${escapeHtml(location.siteNumber || location.id)}</strong><br/>${escapeHtml(location.name || "")}`)
        .on("click", () => selectLocation(location.id))
        .addTo(layer);
      addToBounds(location);
    });

    const heatMax = Math.max(...gridPoints.map((item) => Number(item.concentration) || 0), 0);
    gridPoints.filter(validGeo).forEach((point) => {
      const ratio = heatMax ? (Number(point.concentration) || 0) / heatMax : 0;
      L.circleMarker([Number(point.latitude), Number(point.longitude)], {
        radius: Math.max(3, Math.min(13, 3 + ratio * 12)),
        stroke: false,
        fillColor: "#c98519",
        fillOpacity: Math.max(0.12, Math.min(0.68, ratio))
      }).addTo(layer);
      addToBounds(point);
    });

    if (bounds.length > 1) map.fitBounds(bounds, { padding: [28, 28], maxZoom: 12 });
  }, [locations, gridPoints, selectedLocationId, selectLocation]);

  return <div className="leaflet-map" ref={containerRef} aria-label="Карта OpenStreetMap с пробными площадками" />;
}

function DataView({ locations, metals, samples, selectedLocationId, sampleFilters, setSampleFilters, selectLocation, openLocation, openSample, importSamplesCsv }) {
  return (
    <section className="view is-active">
      <div className="data-grid">
        <section className="panel">
          <div className="panel-head"><h2>Локации</h2><button onClick={openLocation}>+ Добавить</button></div>
          <div className="table-wrap"><table><thead><tr><th>ID</th><th>Площадка</th><th>Номер</th><th>Км</th><th>Широта</th><th>Долгота</th></tr></thead>
            <tbody>{locations.map((item) => <tr key={item.id} className={Number(item.id) === Number(selectedLocationId) ? "is-selected" : ""} onClick={() => selectLocation(item.id)}>
              <td>{item.id}</td><td>{item.name}</td><td>{item.siteNumber}</td><td>{formatNumber(item.distanceFromSourceKm, 2)}</td><td>{formatNumber(item.latitude, 5)}</td><td>{formatNumber(item.longitude, 5)}</td>
            </tr>)}</tbody></table></div>
        </section>
        <section className="panel">
          <div className="panel-head">
            <h2>Пробы</h2>
            <div className="compact-actions"><label className="file-button">CSV<input type="file" accept=".csv,text/csv" onChange={(event) => importSamplesCsv(event.target.files?.[0])} /></label><button onClick={() => openSample(null)}>+ Добавить</button></div>
          </div>
          <div className="filters">
            <Select value={selectedLocationId} onChange={selectLocation} items={locations} label={(item) => `${item.siteNumber} · ${item.name}`} />
            <Select value={sampleFilters.metalId} onChange={(value) => setSampleFilters({ ...sampleFilters, metalId: value })} items={metals} label={(item) => `${item.symbol || item.name} · ${item.name}`} empty="Все металлы" />
            <input type="number" min="1900" max="2100" placeholder="Год" value={sampleFilters.year} onChange={(event) => setSampleFilters({ ...sampleFilters, year: event.target.value })} />
            <button onClick={() => setSampleFilters({ ...sampleFilters })}>Фильтр</button>
          </div>
          <div className="table-wrap"><table><thead><tr><th>ID</th><th>Металл</th><th>Значение</th><th>Дата</th><th>Позиция</th><th>Повт.</th><th /></tr></thead>
            <tbody>{samples.map((item) => <tr key={item.id}><td>{item.id}</td><td>{metalName(metals, item.metalId)}</td><td>{formatNumber(item.value, 6)}</td><td>{item.samplingDate}</td><td>{item.position}</td><td>{item.repetition}</td><td><button className="secondary-button" onClick={() => openSample(item)}>✎</button></td></tr>)}</tbody></table></div>
        </section>
      </div>
    </section>
  );
}

function CalculationDetailsView({ jobs, selectedJobId, currentJob, selectedJobPoints, metals, locations, selectJob, loadJobPoints }) {
  const selectedJob = currentJob?.id === selectedJobId ? currentJob : jobs.find((item) => item.id === selectedJobId);
  return (
    <section className="view is-active">
      <div className="details-layout">
        <section className="panel">
          <div className="panel-head"><div><h2>История расчетов</h2><span>{jobs.length} сохраненных расчетов</span></div></div>
          <div className="table-wrap"><table><thead><tr><th>Создан</th><th>Статус</th><th>Металл</th><th>Год</th><th>Площадка</th></tr></thead>
            <tbody>{jobs.map((job) => <tr key={job.id} className={job.id === selectedJobId ? "is-selected" : ""} onClick={() => selectJob(job.id)}>
              <td>{dateShort(job.createdAt)}</td><td><StatusPill status={job.status} /></td><td>{metalName(metals, job.metalId)}</td><td>{job.year || job.sampleYear}</td><td>{locationLabel(locations, job.referenceLocationId)}</td>
            </tr>)}</tbody></table></div>
        </section>
        <section className="panel">
          <div className="panel-head"><div><h2>Детали расчета</h2><span>{selectedJob?.id || "выбери расчет слева"}</span></div><button onClick={() => loadJobPoints(selectedJob?.id)}>Показать на карте</button></div>
          <Details rows={selectedJob ? [
            ["ID", selectedJob.id],
            ["Статус", <StatusPill status={selectedJob.status} />],
            ["Площадка", locationLabel(locations, selectedJob.referenceLocationId)],
            ["Металл", metalName(metals, selectedJob.metalId)],
            ["Год", selectedJob.year || selectedJob.sampleYear],
            ["Шаг сетки", `${selectedJob.gridStepKm} км`],
            ["Размер области", `${selectedJob.areaSizeKm} км`],
            ["Создан", selectedJob.createdAt],
            ["Завершен", selectedJob.completedAt || ""],
            ["Ошибка", selectedJob.errorMessage || selectedJob.error || "нет"]
          ] : [["Статус", "история пока пустая"]]} />
          <div className="table-wrap points-table"><table><thead><tr><th>Широта</th><th>Долгота</th><th>Концентрация</th></tr></thead>
            <tbody>{selectedJobPoints.map((point, index) => <tr key={`${point.latitude}-${point.longitude}-${index}`}><td>{formatNumber(point.latitude, 6)}</td><td>{formatNumber(point.longitude, 6)}</td><td>{formatNumber(point.concentration, 8)}</td></tr>)}</tbody></table></div>
        </section>
      </div>
    </section>
  );
}

function ReportsView({ jobs, selectedJobId, currentJob, selectedJobPoints, statistics, metals, locations, selectJob }) {
  const selectedJob = currentJob?.id === selectedJobId ? currentJob : jobs.find((item) => item.id === selectedJobId);
  const exportExcel = () => {
    const html = workbookHtml([
      {
        title: "Calculation",
        rows: [
          ["ID", selectedJob?.id || ""],
          ["Статус", statusLabel(selectedJob?.status)],
          ["Площадка", locationLabel(locations, selectedJob?.referenceLocationId)],
          ["Металл", metalName(metals, selectedJob?.metalId)],
          ["Год", selectedJob?.year || selectedJob?.sampleYear || ""]
        ]
      },
      {
        title: "GridPoints",
        rows: [["Широта", "Долгота", "Концентрация"], ...selectedJobPoints.map((point) => [point.latitude, point.longitude, point.concentration])]
      },
      {
        title: "Statistics",
        rows: [["Год", "Площадка", "Металл", "Среднее", "Минимум", "Максимум", "Количество"], ...statistics.map((item) => [item.year, item.siteNumber || item.locationName, item.metalSymbol || item.metalName, item.averageValue, item.minValue, item.maxValue, item.sampleCount])]
      }
    ]);
    downloadBlob(new Blob([html], { type: "application/vnd.ms-excel;charset=utf-8" }), "testdip-report.xls");
  };

  return (
    <section className="view is-active">
      <div className="report-layout">
        <section className="panel no-print">
          <div className="panel-head"><h2>Экспорт</h2></div>
          <label>Расчет<Select value={selectedJobId} onChange={selectJob} items={jobs} label={(job) => `${dateShort(job.createdAt)} · ${statusLabel(job.status)} · ${metalName(metals, job.metalId)}`} empty="Выбери расчет" /></label>
          <div className="report-actions">
            <button onClick={() => window.print()}>PDF</button>
            <button className="secondary-button" onClick={exportExcel}>Excel</button>
          </div>
          <p className="muted">PDF формируется через системную печать браузера, Excel выгружается как табличный файл отчета.</p>
        </section>
        <section className="panel report-print">
          <div className="panel-head"><div><h2>Отчет по загрязнению</h2><span>Расчет концентрации тяжелых металлов</span></div></div>
          <div className="summary-grid">
            <Metric label="Статус" value={statusLabel(selectedJob?.status) || "нет расчета"} />
            <Metric label="Металл" value={metalName(metals, selectedJob?.metalId)} />
            <Metric label="Год" value={selectedJob?.year || selectedJob?.sampleYear || ""} />
            <Metric label="Точек поля" value={selectedJobPoints.length} />
          </div>
          <Details rows={selectedJob ? [
            ["ID расчета", selectedJob.id],
            ["Опорная площадка", locationLabel(locations, selectedJob.referenceLocationId)],
            ["Сетка", `${selectedJob.gridStepKm} км / ${selectedJob.areaSizeKm} км`],
            ["Создан", selectedJob.createdAt],
            ["Завершен", selectedJob.completedAt || ""]
          ] : [["Расчет", "не выбран"]]} />
          <h2>Статистика выборки</h2>
          <div className="table-wrap compact-table"><table><thead><tr><th>Год</th><th>Площадка</th><th>Металл</th><th>Среднее</th><th>Мин.</th><th>Макс.</th><th>n</th></tr></thead>
            <tbody>{statistics.map((item, index) => <tr key={`${item.year}-${item.locationId}-${item.metalId}-${index}`}><td>{item.year}</td><td>{item.siteNumber || item.locationName}</td><td>{item.metalSymbol || item.metalName}</td><td>{formatNumber(item.averageValue, 6)}</td><td>{formatNumber(item.minValue, 6)}</td><td>{formatNumber(item.maxValue, 6)}</td><td>{item.sampleCount}</td></tr>)}</tbody></table></div>
        </section>
      </div>
    </section>
  );
}

function ReferencesView({ locations, metals, calculation, selectedLocationId, selectLocation, openLocation, openMetal, deleteMetal }) {
  return (
    <section className="view is-active">
      <div className="refs-layout">
        <section className="panel">
          <div className="panel-head"><div><h2>Металлы</h2><span>{metals.length} записей справочника</span></div><button onClick={() => openMetal(null)}>+ Добавить</button></div>
          <div className="table-wrap"><table><thead><tr><th>ID</th><th>Символ</th><th>Название</th><th>Ед.</th><th /></tr></thead>
            <tbody>{metals.map((item) => <tr key={item.id}><td>{item.id}</td><td>{item.symbol}</td><td>{item.name}</td><td>{item.unit || ""}</td><td><div className="compact-actions"><button className="secondary-button" onClick={() => openMetal(item)}>✎</button><button className="danger-button" onClick={() => deleteMetal(item)}>×</button></div></td></tr>)}</tbody></table></div>
        </section>
        <section className="panel">
          <div className="panel-head"><div><h2>Локации</h2><span>{locations.length} пробных площадок</span></div><button onClick={openLocation}>+ Добавить</button></div>
          <div className="table-wrap"><table><thead><tr><th>ID</th><th>Номер</th><th>Название</th><th>Км</th><th>Координаты</th></tr></thead>
            <tbody>{locations.map((item) => <tr key={item.id} className={Number(item.id) === Number(selectedLocationId) ? "is-selected" : ""} onClick={() => selectLocation(item.id)}><td>{item.id}</td><td>{item.siteNumber}</td><td>{item.name}</td><td>{formatNumber(item.distanceFromSourceKm, 2)}</td><td>{formatNumber(item.latitude, 5)}, {formatNumber(item.longitude, 5)}</td></tr>)}</tbody></table></div>
        </section>
        <section className="panel">
          <h2>Параметры расчета</h2>
          <Details rows={[
            ["Источник", `${SOURCE_POINT.latitude}, ${SOURCE_POINT.longitude}`],
            ["Шаг по умолчанию", `${calculation.gridStepKm} км`],
            ["Область по умолчанию", `${calculation.areaSizeKm} км`],
            ["Данные", "площадки пробоотбора и концентрации металлов"],
            ["Результат", "поле загрязнения на карте"]
          ]} />
        </section>
      </div>
    </section>
  );
}

function StatusPill({ status }) {
  return <span className={`status-pill status-${String(status || "unknown").replaceAll("_", "-")}`}>{statusLabel(status)}</span>;
}

function Metric({ label, value }) {
  return <div className="metric"><span>{label}</span><strong>{value}</strong></div>;
}

function CalcView({ locations, metals, selectedLocationId, selectLocation, calculation, setCalculation, submitCalculation, currentJob, loadJobPoints }) {
  return (
    <section className="view is-active">
      <div className="calc-grid">
        <section className="panel">
          <h2>Параметры расчета</h2>
          <form className="form-grid" onSubmit={submitCalculation}>
            <label>Опорная площадка<Select value={selectedLocationId} onChange={selectLocation} items={locations} label={(item) => `${item.siteNumber} · ${item.name}`} /></label>
            <label>Металл<Select value={calculation.metalId} onChange={(value) => setCalculation({ ...calculation, metalId: value })} items={metals} label={(item) => `${item.symbol || item.name} · ${item.name}`} /></label>
            <label>Год<input type="number" min="1900" max="2100" value={calculation.year} onChange={(event) => setCalculation({ ...calculation, year: event.target.value })} /></label>
            <label>Шаг сетки, км<input type="number" min="0.1" step="0.1" value={calculation.gridStepKm} onChange={(event) => setCalculation({ ...calculation, gridStepKm: event.target.value })} /></label>
            <label>Размер области, км<input type="number" min="5" step="1" value={calculation.areaSizeKm} onChange={(event) => setCalculation({ ...calculation, areaSizeKm: event.target.value })} /></label>
            <button type="submit">Рассчитать</button>
          </form>
        </section>
        <section className="panel">
          <div className="panel-head"><h2>Результат расчета</h2><button onClick={loadJobPoints}>Показать точки</button></div>
          <Details rows={currentJob ? [
            ["ID", currentJob.id],
            ["Статус", statusLabel(currentJob.status)],
            ["Площадка", currentJob.referenceLocationId],
            ["Металл", metalName(metals, currentJob.metalId)],
            ["Год", currentJob.year],
            ["Сетка", `${currentJob.gridStepKm} км · ${currentJob.areaSizeKm} км`],
            ["Ошибка", currentJob.errorMessage || currentJob.error || "нет"]
          ] : [["Статус", "нет активной задачи"]]} />
          <p className="muted">После завершения расчета результат можно отобразить на карте и использовать в отчете.</p>
        </section>
      </div>
    </section>
  );
}

function StatsView({ locations, metals, statistics, statsFilters, setStatsFilters, loadStatistics }) {
  return (
    <section className="view is-active">
      <div className="stats-layout">
        <section className="panel">
          <div className="panel-head"><h2>Динамика концентраций</h2><button onClick={loadStatistics}>Обновить</button></div>
          <div className="filters">
            <Select value={statsFilters.locationId} onChange={(value) => setStatsFilters({ ...statsFilters, locationId: value })} items={locations} label={(item) => `${item.siteNumber} · ${item.name}`} empty="Все площадки" />
            <Select value={statsFilters.metalId} onChange={(value) => setStatsFilters({ ...statsFilters, metalId: value })} items={metals} label={(item) => `${item.symbol || item.name} · ${item.name}`} empty="Все металлы" />
            <input type="number" placeholder="С года" value={statsFilters.yearFrom} onChange={(event) => setStatsFilters({ ...statsFilters, yearFrom: event.target.value })} />
            <input type="number" placeholder="По год" value={statsFilters.yearTo} onChange={(event) => setStatsFilters({ ...statsFilters, yearTo: event.target.value })} />
            <button onClick={loadStatistics}>Анализ</button>
          </div>
          <TrendChart statistics={statistics} />
          <div className="table-wrap compact-table"><table><thead><tr><th>Год</th><th>Площадка</th><th>Металл</th><th>Среднее</th><th>Мин.</th><th>Макс.</th><th>n</th></tr></thead>
            <tbody>{statistics.map((item, index) => <tr key={`${item.year}-${item.locationId}-${item.metalId}-${index}`}><td>{item.year}</td><td>{item.siteNumber || item.locationName}</td><td>{item.metalSymbol || item.metalName}</td><td>{formatNumber(item.averageValue, 6)}</td><td>{formatNumber(item.minValue, 6)}</td><td>{formatNumber(item.maxValue, 6)}</td><td>{item.sampleCount}</td></tr>)}</tbody></table></div>
        </section>
        <section className="panel"><h2>Справочник металлов</h2><div className="metal-list">{metals.map((item) => <div className="metal-chip" key={item.id}><strong>{item.symbol || item.name}</strong><span>{item.name} {item.unit ? `· ${item.unit}` : ""}</span></div>)}</div></section>
      </div>
    </section>
  );
}

function TrendChart({ statistics }) {
  const canvasRef = useRef(null);
  useEffect(() => {
    const canvas = canvasRef.current;
    const context = canvas.getContext("2d");
    drawTrend(context, canvas, statistics);
  }, [statistics]);
  return <canvas ref={canvasRef} width="900" height="320" />;
}

function LocationDialog({ location, onClose, onSave }) {
  const [form, setForm] = useState(location || { name: "", siteNumber: "", distanceFromSourceKm: 0, latitude: "", longitude: "", description: "" });
  return <Modal title={location ? "Редактирование площадки" : "Новая площадка"} onClose={onClose} onSave={() => onSave({ ...form, distanceFromSourceKm: Number(form.distanceFromSourceKm), latitude: Number(form.latitude), longitude: Number(form.longitude) })}>
    <label>Название<input value={form.name} onChange={(event) => setForm({ ...form, name: event.target.value })} /></label>
    <label>Номер площадки<input value={form.siteNumber} onChange={(event) => setForm({ ...form, siteNumber: event.target.value })} /></label>
    <label>Расстояние от источника, км<input type="number" value={form.distanceFromSourceKm} onChange={(event) => setForm({ ...form, distanceFromSourceKm: event.target.value })} /></label>
    <label>Широта<input type="number" step="0.000001" value={form.latitude} onChange={(event) => setForm({ ...form, latitude: event.target.value })} /></label>
    <label>Долгота<input type="number" step="0.000001" value={form.longitude} onChange={(event) => setForm({ ...form, longitude: event.target.value })} /></label>
    <label className="wide">Описание<input value={form.description || ""} onChange={(event) => setForm({ ...form, description: event.target.value })} /></label>
  </Modal>;
}

function MetalDialog({ metal, onClose, onSave }) {
  const [form, setForm] = useState(metal || { name: "", symbol: "", unit: "" });
  return <Modal title={metal ? "Редактирование металла" : "Новый металл"} onClose={onClose} onSave={() => onSave({ ...form, name: form.name.trim(), symbol: form.symbol.trim(), unit: form.unit?.trim() || "" })}>
    <label>Название<input required value={form.name} onChange={(event) => setForm({ ...form, name: event.target.value })} /></label>
    <label>Символ<input required value={form.symbol} onChange={(event) => setForm({ ...form, symbol: event.target.value })} /></label>
    <label className="wide">Единица измерения<input value={form.unit || ""} onChange={(event) => setForm({ ...form, unit: event.target.value })} /></label>
  </Modal>;
}

function SampleDialog({ sample, locations, metals, selectedLocationId, onClose, onSave }) {
  const [form, setForm] = useState(sample || { locationId: selectedLocationId || locations[0]?.id || "", metalId: metals[0]?.id || "", value: "", samplingDate: new Date().toISOString().slice(0, 10), position: "", repetition: 1, analyticsNumber: "" });
  return <Modal title={sample ? "Редактирование пробы" : "Новая проба"} onClose={onClose} onSave={() => onSave({ ...form, locationId: Number(form.locationId), metalId: Number(form.metalId), value: Number(form.value), repetition: Number(form.repetition) })}>
    <label>Локация<Select value={form.locationId} onChange={(value) => setForm({ ...form, locationId: value })} items={locations} label={(item) => `${item.siteNumber} · ${item.name}`} /></label>
    <label>Металл<Select value={form.metalId} onChange={(value) => setForm({ ...form, metalId: value })} items={metals} label={(item) => `${item.symbol || item.name} · ${item.name}`} /></label>
    <label>Значение<input type="number" step="0.000001" value={form.value} onChange={(event) => setForm({ ...form, value: event.target.value })} /></label>
    <label>Дата отбора<input type="date" value={form.samplingDate} onChange={(event) => setForm({ ...form, samplingDate: event.target.value })} /></label>
    <label>Позиция<input value={form.position || ""} onChange={(event) => setForm({ ...form, position: event.target.value })} /></label>
    <label>Повторность<input type="number" value={form.repetition} onChange={(event) => setForm({ ...form, repetition: event.target.value })} /></label>
    <label className="wide">Номер аналитики<input value={form.analyticsNumber || ""} onChange={(event) => setForm({ ...form, analyticsNumber: event.target.value })} /></label>
  </Modal>;
}

function Modal({ title, children, onClose, onSave }) {
  return <div className="modal-backdrop"><form className="dialog-form" onSubmit={(event) => { event.preventDefault(); onSave(); }}><h2>{title}</h2><div className="form-grid">{children}</div><menu><button type="button" className="secondary-button" onClick={onClose}>Отмена</button><button type="submit">Сохранить</button></menu></form></div>;
}

function Select({ value, onChange, items, label, empty }) {
  return <select value={value || ""} onChange={(event) => onChange(event.target.value)}>{empty && <option value="">{empty}</option>}{items.map((item) => <option key={item.id} value={item.id}>{label(item)}</option>)}</select>;
}

function Details({ rows }) {
  return <dl className="details-list">{rows.map(([key, value]) => <React.Fragment key={key}><dt>{key}</dt><dd>{value ?? ""}</dd></React.Fragment>)}</dl>;
}

function createApiClient(apiBase) {
  const base = apiBase.replace(/\/$/, "");
  const request = async (path, options = {}) => {
    const response = await fetch(`${base}${path}`, { headers: { "Content-Type": "application/json" }, ...options });
    if (response.status === 204) return null;
    const text = await response.text();
    const payload = text ? JSON.parse(text) : null;
    if (!response.ok) throw new Error(payload?.error || `HTTP ${response.status}`);
    return payload;
  };
  return {
    get: (path) => request(path),
    post: (path, body) => request(path, { method: "POST", body: JSON.stringify(body) }),
    put: (path, body) => request(path, { method: "PUT", body: JSON.stringify(body) }),
    delete: (path) => request(path, { method: "DELETE" })
  };
}

function buildMap(locations, gridPoints, selectedLocationId) {
  const width = 960;
  const height = 600;
  const mapLocations = locations.filter(validGeo);
  const points = [...mapLocations, SOURCE_POINT, ...gridPoints.filter(validGeo)];
  const bounds = boundsFor(points);
  const project = (point) => ({
    x: ((point.longitude - bounds.minLon) / (bounds.maxLon - bounds.minLon || 1)) * (width - 80) + 40,
    y: height - (((point.latitude - bounds.minLat) / (bounds.maxLat - bounds.minLat || 1)) * (height - 80) + 40)
  });
  const heatMax = Math.max(...gridPoints.map((item) => item.concentration), 0);
  const heat = gridPoints.filter(validGeo).map((point) => {
    const { x, y } = project(point);
    const ratio = heatMax ? point.concentration / heatMax : 0;
    return `<circle cx="${x}" cy="${y}" r="${Math.max(4, Math.min(18, 4 + ratio * 16))}" fill="#d8901d" opacity="${Math.max(0.08, Math.min(0.72, ratio))}"></circle>`;
  }).join("");
  const locationNodes = mapLocations.map((location) => {
    const { x, y } = project(location);
    const selected = Number(location.id) === Number(selectedLocationId);
    return `<g data-map-location="${location.id}"><circle cx="${x}" cy="${y}" r="${selected ? 8 : 6}" fill="${selected ? "#0c5f55" : "#157a6e"}" stroke="#fff" stroke-width="2"></circle><text x="${x + 10}" y="${y - 8}">${escapeSvg(location.siteNumber || location.id)}</text></g>`;
  }).join("");
  const plant = project(SOURCE_POINT);
  return `<defs><pattern id="grid" width="48" height="48" patternUnits="userSpaceOnUse"><path d="M 48 0 L 0 0 0 48" fill="none" stroke="#ccd8d3" stroke-width="1"></path></pattern></defs><rect x="0" y="0" width="${width}" height="${height}" fill="url(#grid)"></rect>${heat}<g><circle cx="${plant.x}" cy="${plant.y}" r="12" fill="#b84a3a" stroke="#fff" stroke-width="3"></circle><text x="${plant.x + 16}" y="${plant.y + 4}" class="plant-label">СЕВЕРОНИКЕЛЬ</text></g>${locationNodes}`;
}

function drawTrend(context, canvas, statistics) {
  context.clearRect(0, 0, canvas.width, canvas.height);
  context.fillStyle = "#fff";
  context.fillRect(0, 0, canvas.width, canvas.height);
  const grouped = new Map();
  statistics.forEach((item) => {
    if (!grouped.has(item.year)) grouped.set(item.year, []);
    grouped.get(item.year).push(Number(item.averageValue) || 0);
  });
  const data = Array.from(grouped.entries()).map(([year, values]) => ({ year: Number(year), value: values.reduce((sum, item) => sum + item, 0) / values.length })).sort((a, b) => a.year - b.year);
  context.fillStyle = "#65736d";
  context.font = "14px system-ui";
  if (!data.length) {
    context.fillText("Нет данных для графика", 28, 42);
    return;
  }
  const pad = 42;
  const max = Math.max(...data.map((item) => item.value), 1);
  const minYear = data[0].year;
  const maxYear = data[data.length - 1].year;
  const xFor = (year) => pad + ((year - minYear) / (maxYear - minYear || 1)) * (canvas.width - pad * 2);
  const yFor = (value) => canvas.height - pad - (value / max) * (canvas.height - pad * 2);
  context.strokeStyle = "#d8dfdc";
  context.lineWidth = 1;
  context.beginPath();
  context.moveTo(pad, pad);
  context.lineTo(pad, canvas.height - pad);
  context.lineTo(canvas.width - pad, canvas.height - pad);
  context.stroke();
  context.strokeStyle = "#157a6e";
  context.lineWidth = 3;
  context.beginPath();
  data.forEach((item, index) => index ? context.lineTo(xFor(item.year), yFor(item.value)) : context.moveTo(xFor(item.year), yFor(item.value)));
  context.stroke();
  context.fillStyle = "#157a6e";
  data.forEach((item) => {
    const x = xFor(item.year);
    const y = yFor(item.value);
    context.beginPath();
    context.arc(x, y, 5, 0, Math.PI * 2);
    context.fill();
    context.fillText(String(item.year), x - 14, canvas.height - 16);
  });
}

function drawStaticMap(context, canvas, locations, gridPoints, selectedLocationId) {
  context.clearRect(0, 0, canvas.width, canvas.height);
  context.fillStyle = "#f6f8f7";
  context.fillRect(0, 0, canvas.width, canvas.height);
  context.strokeStyle = "#d8dfdc";
  context.lineWidth = 1;
  for (let x = 40; x < canvas.width; x += 60) {
    context.beginPath();
    context.moveTo(x, 0);
    context.lineTo(x, canvas.height);
    context.stroke();
  }
  for (let y = 40; y < canvas.height; y += 60) {
    context.beginPath();
    context.moveTo(0, y);
    context.lineTo(canvas.width, y);
    context.stroke();
  }

  const mapLocations = locations.filter(validGeo);
  const points = [...mapLocations, SOURCE_POINT, ...gridPoints.filter(validGeo)];
  const bounds = boundsFor(points);
  const project = (point) => ({
    x: ((Number(point.longitude) - bounds.minLon) / (bounds.maxLon - bounds.minLon || 1)) * (canvas.width - 110) + 55,
    y: canvas.height - (((Number(point.latitude) - bounds.minLat) / (bounds.maxLat - bounds.minLat || 1)) * (canvas.height - 110) + 55)
  });

  const heatMax = Math.max(...gridPoints.map((item) => Number(item.concentration) || 0), 0);
  gridPoints.filter(validGeo).forEach((point) => {
    const { x, y } = project(point);
    const ratio = heatMax ? (Number(point.concentration) || 0) / heatMax : 0;
    context.fillStyle = `rgba(201, 133, 25, ${Math.max(0.1, Math.min(0.66, ratio))})`;
    context.beginPath();
    context.arc(x, y, Math.max(4, Math.min(18, 4 + ratio * 16)), 0, Math.PI * 2);
    context.fill();
  });

  const plant = project(SOURCE_POINT);
  context.fillStyle = "#b84a3a";
  context.beginPath();
  context.arc(plant.x, plant.y, 12, 0, Math.PI * 2);
  context.fill();
  context.fillStyle = "#17211d";
  context.font = "18px system-ui";
  context.fillText("СЕВЕРОНИКЕЛЬ", plant.x + 18, plant.y + 6);

  mapLocations.forEach((location) => {
    const { x, y } = project(location);
    const selected = Number(location.id) === Number(selectedLocationId);
    context.fillStyle = selected ? "#0c5f55" : "#157a6e";
    context.beginPath();
    context.arc(x, y, selected ? 9 : 7, 0, Math.PI * 2);
    context.fill();
    context.fillStyle = "#17211d";
    context.font = "15px system-ui";
    context.fillText(String(location.siteNumber || location.id), x + 11, y - 8);
  });
}

function boundsFor(points) {
  const items = points.length ? points : [SOURCE_POINT];
  const latitudes = items.map((item) => Number(item.latitude)).filter(Number.isFinite);
  const longitudes = items.map((item) => Number(item.longitude)).filter(Number.isFinite);
  return {
    minLat: Math.min(...latitudes) - 0.02,
    maxLat: Math.max(...latitudes) + 0.02,
    minLon: Math.min(...longitudes) - 0.02,
    maxLon: Math.max(...longitudes) + 0.02
  };
}

function validGeo(point) {
  const latitude = Number(point.latitude);
  const longitude = Number(point.longitude);
  return Number.isFinite(latitude) && Number.isFinite(longitude) && Math.abs(latitude) <= 90 && Math.abs(longitude) <= 180 && !(latitude === 0 && longitude === 0);
}

function parseCsv(text) {
  const rows = [];
  let row = [];
  let cell = "";
  let quoted = false;
  for (let index = 0; index < text.length; index += 1) {
    const char = text[index];
    const next = text[index + 1];
    if (char === '"' && quoted && next === '"') {
      cell += '"';
      index += 1;
    } else if (char === '"') {
      quoted = !quoted;
    } else if (char === "," && !quoted) {
      row.push(cell.trim());
      cell = "";
    } else if ((char === "\n" || char === "\r") && !quoted) {
      if (char === "\r" && next === "\n") index += 1;
      row.push(cell.trim());
      if (row.some(Boolean)) rows.push(row);
      row = [];
      cell = "";
    } else {
      cell += char;
    }
  }
  row.push(cell.trim());
  if (row.some(Boolean)) rows.push(row);
  return rows;
}

function metalName(metals, id) {
  const metal = metals.find((item) => Number(item.id) === Number(id));
  return metal ? `${metal.symbol || metal.name}` : `#${id}`;
}

function locationLabel(locations, id) {
  const location = locations.find((item) => Number(item.id) === Number(id));
  return location ? `${location.siteNumber} · ${location.name}` : id ? `#${id}` : "";
}

function dateShort(value) {
  if (!value) return "";
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return String(value);
  return date.toLocaleString("ru-RU", { year: "numeric", month: "2-digit", day: "2-digit", hour: "2-digit", minute: "2-digit" });
}

function statusLabel(status) {
  const labels = {
    queued: "в очереди",
    processing: "выполняется",
    completed: "завершен",
    failed: "ошибка",
    publish_failed: "ошибка запуска"
  };
  return labels[status] || status || "";
}

function formatNumber(value, digits) {
  const number = Number(value);
  return Number.isFinite(number) ? number.toFixed(digits) : "";
}

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, (char) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#039;" }[char]));
}

function escapeSvg(value) {
  return String(value).replace(/[&<>"']/g, (char) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#039;" }[char]));
}

function downloadCsv(rows, filename) {
  const csv = rows.map((row) => row.map((value) => `"${String(value).replaceAll('"', '""')}"`).join(",")).join("\n");
  downloadBlob(new Blob([csv], { type: "text/csv;charset=utf-8" }), filename);
}

function workbookHtml(sheets) {
  return `<!doctype html><html><head><meta charset="utf-8"></head><body>${sheets.map((sheet) => `<h2>${escapeHtml(sheet.title)}</h2><table border="1">${sheet.rows.map((row) => `<tr>${row.map((cell) => `<td>${escapeHtml(cell ?? "")}</td>`).join("")}</tr>`).join("")}</table>`).join("<br/>")}</body></html>`;
}

function downloadBlob(blob, filename) {
  if (!blob) return;
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = filename;
  link.click();
  URL.revokeObjectURL(url);
}

function delay(ms) {
  return new Promise((resolve) => window.setTimeout(resolve, ms));
}

createRoot(document.getElementById("root")).render(<App />);
